#include "monggle/auth/auth_service.h"
#include "monggle/auth/jwt_service.h"
#include "monggle/auth/password_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <chrono>

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

        db()->execSqlSync(
            "INSERT INTO refresh_tokens (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
            userId,
            sha256Hex(pair.refreshToken),
            toMysqlDateTime(pair.refreshExpiresAt));

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

        db()->execSqlSync(
            "INSERT INTO refresh_tokens (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
            userId,
            sha256Hex(pair.refreshToken),
            toMysqlDateTime(pair.refreshExpiresAt));

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
        auto rows = db()->execSqlSync(
            "SELECT id, user_id FROM refresh_tokens "
            "WHERE token_hash = ? AND revoked = 0 AND expires_at > NOW(3) LIMIT 1",
            hash);
        if (rows.size() == 0) {
            return AuthError{AuthError::InvalidRefresh, "refresh token not found or expired"};
        }
        auto rtId   = rows[0]["id"].as<std::int64_t>();
        auto userId = rows[0]["user_id"].as<std::int64_t>();

        // 회전: 기존 토큰 폐기, 새 페어 발급
        db()->execSqlSync("UPDATE refresh_tokens SET revoked = 1 WHERE id = ?", rtId);

        std::string jti;
        TokenPair pair;
        pair.userId           = userId;
        pair.accessToken      = jwt_->issueAccess(userId);
        pair.refreshToken     = jwt_->issueRefresh(userId, jti);
        pair.accessExpiresAt  = nowEpoch() + jwt_->accessTtl().count();
        pair.refreshExpiresAt = nowEpoch() + jwt_->refreshTtl().count();

        db()->execSqlSync(
            "INSERT INTO refresh_tokens (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
            userId,
            sha256Hex(pair.refreshToken),
            toMysqlDateTime(pair.refreshExpiresAt));

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
