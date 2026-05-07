#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace monggle {

struct JwtClaims {
    std::int64_t userId;
    std::string  tokenType;     // "access" | "refresh"
    std::string  jti;           // JWT ID, refresh token 회전·취소용
    std::int64_t expiresAt;     // unix epoch seconds
};

class JwtService {
public:
    // privateKeyPem: RS256 개인키 PEM (서명/검증 모두에 keypair 사용)
    // publicKeyPem : RS256 공개키 PEM (검증 전용)
    JwtService(std::string privateKeyPem,
               std::string publicKeyPem,
               std::string issuer,
               std::chrono::seconds accessTtl,
               std::chrono::seconds refreshTtl);

    std::string issueAccess(std::int64_t userId);
    std::string issueRefresh(std::int64_t userId, std::string& outJti);

    std::optional<JwtClaims> verify(const std::string& token, const std::string& expectedType) const;

    std::chrono::seconds accessTtl() const { return accessTtl_; }
    std::chrono::seconds refreshTtl() const { return refreshTtl_; }

private:
    std::string privateKeyPem_;
    std::string publicKeyPem_;
    std::string issuer_;
    std::chrono::seconds accessTtl_;
    std::chrono::seconds refreshTtl_;
};

// 파일에서 PEM을 읽는 유틸
std::string readPemFile(const std::string& path);

// 비교용 32바이트 hex (sha256) — refresh_tokens.token_hash 계산
std::string sha256Hex(const std::string& input);

} // namespace monggle
