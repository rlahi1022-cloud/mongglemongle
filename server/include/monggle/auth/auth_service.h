#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace sw { namespace redis { class Redis; } }

namespace monggle {

class JwtService;

struct AuthError {
    enum Code { EmailTaken, InvalidCredentials, InvalidRefresh, InternalError };
    Code        code;
    std::string detail;
};

struct TokenPair {
    std::string  accessToken;
    std::string  refreshToken;
    std::int64_t userId;
    std::int64_t accessExpiresAt;
    std::int64_t refreshExpiresAt;
};

using AuthResult = std::variant<TokenPair, AuthError>;

class AuthService {
public:
    explicit AuthService(std::shared_ptr<JwtService> jwt);

    // 모두 동기적인 단순 구현. drogon 핸들러에서 thread pool로 위임 가능.
    AuthResult signup(const std::string& email,
                      const std::string& password,
                      const std::string& displayName);

    AuthResult login(const std::string& email,
                     const std::string& password);

    AuthResult refresh(const std::string& refreshToken);

    bool logout(const std::string& refreshToken);

    // Authorization 헤더의 "Bearer ..." 에서 user_id 추출 (실패 시 nullopt)
    std::optional<std::int64_t> verifyAccess(const std::string& bearerHeader) const;

    // Refresh token allowlist를 Redis로 운영. issue/validate/rotate/revoke 모두
    // Redis fast path를 우선 사용하고, 끊기면 DB fallback. 미설정이면 DB만 사용.
    void setRedis(std::shared_ptr<sw::redis::Redis> redis);
    bool redisHealthy() const { return redisHealthy_.load(); }

private:
    void rtRedisAllow(const std::string& tokenHash, std::int64_t userId, std::chrono::seconds ttl);
    void rtRedisRevoke(const std::string& tokenHash);
    std::optional<std::int64_t> rtRedisLookup(const std::string& tokenHash);

    std::shared_ptr<JwtService>        jwt_;
    std::shared_ptr<sw::redis::Redis>  redis_;
    std::atomic<bool>                  redisHealthy_{false};
};

} // namespace monggle
