#include "monggle/auth/jwt_service.h"

#include <jwt/jwt.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace monggle {

namespace {

std::string randomJti() {
    std::array<unsigned char, 16> raw{};
    if (RAND_bytes(raw.data(), raw.size()) != 1) {
        throw std::runtime_error("RAND_bytes failed for jti");
    }
    char hex[33];
    for (int i = 0; i < 16; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", raw[i]);
    }
    return std::string(hex, 32);
}

} // namespace

std::string readPemFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read PEM file: " + path);
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string sha256Hex(const std::string& input) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
    char hex[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    return std::string(hex, SHA256_DIGEST_LENGTH * 2);
}

JwtService::JwtService(std::string privateKeyPem,
                       std::string publicKeyPem,
                       std::string issuer,
                       std::chrono::seconds accessTtl,
                       std::chrono::seconds refreshTtl)
    : privateKeyPem_(std::move(privateKeyPem)),
      publicKeyPem_(std::move(publicKeyPem)),
      issuer_(std::move(issuer)),
      accessTtl_(accessTtl),
      refreshTtl_(refreshTtl) {}

std::string JwtService::issueAccess(std::int64_t userId) {
    jwt::jwt_object obj{
        jwt::params::algorithm("RS256"),
        jwt::params::secret(privateKeyPem_)};
    auto now = std::chrono::system_clock::now();
    auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto exp = std::chrono::duration_cast<std::chrono::seconds>(
                   (now + accessTtl_).time_since_epoch()).count();
    obj.add_claim("iss", issuer_)
       .add_claim("sub", std::to_string(userId))
       .add_claim("typ", std::string("access"))
       .add_claim("iat", iat)
       .add_claim("exp", exp);
    return obj.signature();
}

std::string JwtService::issueRefresh(std::int64_t userId, std::string& outJti) {
    outJti = randomJti();
    jwt::jwt_object obj{
        jwt::params::algorithm("RS256"),
        jwt::params::secret(privateKeyPem_)};
    auto now = std::chrono::system_clock::now();
    auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto exp = std::chrono::duration_cast<std::chrono::seconds>(
                   (now + refreshTtl_).time_since_epoch()).count();
    obj.add_claim("iss", issuer_)
       .add_claim("sub", std::to_string(userId))
       .add_claim("typ", std::string("refresh"))
       .add_claim("jti", outJti)
       .add_claim("iat", iat)
       .add_claim("exp", exp);
    return obj.signature();
}

std::optional<JwtClaims> JwtService::verify(const std::string& token,
                                            const std::string& expectedType) const {
    try {
        auto decoded = jwt::decode(
            token,
            jwt::params::algorithms({"RS256"}),
            jwt::params::secret(publicKeyPem_),
            jwt::params::verify(true),
            jwt::params::issuer(issuer_));

        const auto& payload = decoded.payload();

        // 클레임 직접 접근 — cpp-jwt 1.4
        std::string typ = payload.get_claim_value<std::string>("typ");
        if (typ != expectedType) return std::nullopt;

        JwtClaims c;
        c.userId    = std::stoll(payload.get_claim_value<std::string>("sub"));
        c.tokenType = typ;
        c.jti       = payload.has_claim("jti")
                          ? payload.get_claim_value<std::string>("jti")
                          : std::string{};
        c.expiresAt = payload.get_claim_value<std::int64_t>("exp");
        return c;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace monggle
