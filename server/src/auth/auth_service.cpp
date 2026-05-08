#include "monggle/auth/auth_service.h"
#include "monggle/auth/jwt_service.h"
#include "monggle/auth/password_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <sw/redis++/redis++.h>

#include <chrono>
#include <iostream>

namespace monggle {

namespace {

drogon::orm::DbClientPtr db() {
    return drogon::app().getDbClient("monggle_db");
}

std::int64_t nowEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string toMysqlDateTime(std::int64_t epochSeconds) {
    std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf) + ".000";
}

} // namespace

AuthService::AuthService(std::shared_ptr<JwtService> jwt) : jwt_(std::move(jwt)) {}

void AuthService::setRedis(std::shared_ptr<sw::redis::Redis> redis) {
    if (!redis) {
        redis_.reset();
        redisHealthy_.store(false);
        return;
    }
    try {
        redis->ping();
        redis_ = std::move(redis);
        redisHealthy_.store(true);
    } catch (const std::exception& e) {
        std::cerr << "[auth] redis ping failed: " << e.what()
                  << " — refresh tokens will use DB-only" << std::endl;
        redisHealthy_.store(false);
    }
}

void AuthService::rtRedisAllow(const std::string& tokenHash, std::int64_t userId,
                               std::chrono::seconds ttl) {
    if (!redisHealthy_.load() || !redis_) return;
    try {
        redis_->set("rt:" + tokenHash, std::to_string(userId), ttl);
    } catch (const std::exception& e) {
        std::cerr << "[auth] redis allow error: " << e.what() << std::endl;
        redisHealthy_.store(false);
    }
}

void AuthService::rtRedisRevoke(const std::string& tokenHash) {
    if (!redisHealthy_.load() || !redis_) return;
    try {
        redis_->del("rt:" + tokenHash);
    } catch (const std::exception& e) {
        std::cerr << "[auth] redis revoke error: " << e.what() << std::endl;
        redisHealthy_.store(false);
    }
}

std::optional<std::int64_t> AuthService::rtRedisLookup(const std::string& tokenHash) {
    if (!redisHealthy_.load() || !redis_) return std::nullopt;
    try {
        auto v = redis_->get("rt:" + tokenHash);
        if (!v) return std::nullopt;
        return std::stoll(*v);
    } catch (const std::exception& e) {
        std::cerr << "[auth] redis lookup error: " << e.what() << std::endl;
        redisHealthy_.store(false);
        return std::nullopt;
    }
}

AuthResult AuthService::signup(const std::string& email,
                               const std::string& password,
                               const std::string& displayName) {
    try {
        auto exists = db()->execSqlSync(
            "SELECT id FROM users WHERE email = ? LIMIT 1", email);
        if (exists.size() > 0) {
            return AuthError{AuthError::EmailTaken, "email already registered"};
        }

        auto hashed = PasswordService::hash(password);

        auto inserted = db()->execSqlSync(
            "INSERT INTO users (email, password_hash, display_name) VALUES (?, ?, ?)",
            email, hashed, displayName);

        std::int64_t userId = inserted.insertId();

        std::string jti;
        TokenPair pair;
        pair.userId           = userId;
        pair.accessToken      = jwt_->issueAccess(userId);
        pair.refreshToken     = jwt_->issueRefresh(userId, jti);
        pair.accessExpiresAt  = nowEpoch() + jwt_->accessTtl().count();
        pair.refreshExpiresAt = nowEpoch() + jwt_->refreshTtl().count();

        auto rtHash = sha256Hex(pair.refreshToken);
        db()->execSqlSync(
            "INSERT INTO refresh_tokens (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
            userId,
            rtHash,
            toMysqlDateTime(pair.refreshExpiresAt));
        rtRedisAllow(rtHash, userId, jwt_->refreshTtl());

        return pair;
    } catch (const drogon::orm::DrogonDbException& e) {
        return AuthError{AuthError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return AuthError{AuthError::InternalError, e.what()};
    }
}

AuthResult AuthService::login(const std::string& email, const std::string& password) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT id, password_hash FROM users WHERE email = ? LIMIT 1", email);
        if (rows.size() == 0) {
            return AuthError{AuthError::InvalidCredentials, "invalid email or password"};
        }
        auto userId = rows[0]["id"].as<std::int64_t>();
        auto hashed = rows[0]["password_hash"].as<std::string>();

        if (!PasswordService::verify(password, hashed)) {
            return AuthError{AuthError::InvalidCredentials, "invalid email or password"};
        }

        std::string jti;
        TokenPair pair;
        pair.userId           = userId;
        pair.accessToken      = jwt_->issueAccess(userId);
        pair.refreshToken     = jwt_->issueRefresh(userId, jti);
        pair.accessExpiresAt  = nowEpoch() + jwt_->accessTtl().count();
        pair.refreshExpiresAt = nowEpoch() + jwt_->refreshTtl().count();

        auto rtHash = sha256Hex(pair.refreshToken);
        db()->execSqlSync(
            "INSERT INTO refresh_tokens (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
            userId,
            rtHash,
            toMysqlDateTime(pair.refreshExpiresAt));
        rtRedisAllow(rtHash, userId, jwt_->refreshTtl());

        return pair;
    } catch (const drogon::orm::DrogonDbException& e) {
        return AuthError{AuthError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return AuthError{AuthError::InternalError, e.what()};
    }
}

AuthResult AuthService::refresh(const std::string& refreshToken) {
    auto claims = jwt_->verify(refreshToken, "refresh");
    if (!claims) {
        return AuthError{AuthError::InvalidRefresh, "invalid refresh token"};
    }
    try {
        auto hash = sha256Hex(refreshToken);

        // Fast path: Redis allowlist에 hash가 있으면 user_id를 즉시 얻음.
        // Redis 다운/미존재 시 DB로 fallback.
        std::int64_t userId = -1;
        std::int64_t rtRowId = -1;
        if (auto cached = rtRedisLookup(hash)) {
            userId = *cached;
        } else {
            auto rows = db()->execSqlSync(
                "SELECT id, user_id FROM refresh_tokens "
                "WHERE token_hash = ? AND revoked = 0 AND expires_at > NOW(3) LIMIT 1",
                hash);
            if (rows.size() == 0) {
                return AuthError{AuthError::InvalidRefresh, "refresh token not found or expired"};
            }
            rtRowId = rows[0]["id"].as<std::int64_t>();
            userId  = rows[0]["user_id"].as<std::int64_t>();
        }

        // 회전: 기존 토큰 폐기 (DB + Redis), 새 페어 발급
        if (rtRowId > 0) {
            db()->execSqlSync("UPDATE refresh_tokens SET revoked = 1 WHERE id = ?", rtRowId);
        } else {
            db()->execSqlSync("UPDATE refresh_tokens SET revoked = 1 WHERE token_hash = ?", hash);
        }
        rtRedisRevoke(hash);

        std::string jti;
        TokenPair pair;
        pair.userId           = userId;
        pair.accessToken      = jwt_->issueAccess(userId);
        pair.refreshToken     = jwt_->issueRefresh(userId, jti);
        pair.accessExpiresAt  = nowEpoch() + jwt_->accessTtl().count();
        pair.refreshExpiresAt = nowEpoch() + jwt_->refreshTtl().count();

        auto newHash = sha256Hex(pair.refreshToken);
        db()->execSqlSync(
            "INSERT INTO refresh_tokens (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
            userId,
            newHash,
            toMysqlDateTime(pair.refreshExpiresAt));
        rtRedisAllow(newHash, userId, jwt_->refreshTtl());

        return pair;
    } catch (const drogon::orm::DrogonDbException& e) {
        return AuthError{AuthError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return AuthError{AuthError::InternalError, e.what()};
    }
}

bool AuthService::logout(const std::string& refreshToken) {
    try {
        auto hash = sha256Hex(refreshToken);
        auto affected = db()->execSqlSync(
            "UPDATE refresh_tokens SET revoked = 1 WHERE token_hash = ?", hash);
        rtRedisRevoke(hash);
        return affected.affectedRows() > 0;
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<std::int64_t> AuthService::verifyAccess(const std::string& bearerHeader) const {
    constexpr std::string_view prefix{"Bearer "};
    if (bearerHeader.size() <= prefix.size() ||
        bearerHeader.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }
    auto token = bearerHeader.substr(prefix.size());
    auto claims = jwt_->verify(token, "access");
    if (!claims) return std::nullopt;
    return claims->userId;
}

} // namespace monggle
