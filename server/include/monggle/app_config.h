#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace monggle {

struct AppConfig {
    std::string httpHost;
    std::uint16_t httpPort;

    std::string dbHost;
    std::uint16_t dbPort;
    std::string dbName;
    std::string dbUser;
    std::string dbPassword;
    std::size_t dbPoolSize;

    std::string redisHost;
    std::uint16_t redisPort;
    std::size_t redisPoolSize;

    std::string jwtIssuer;
    std::string jwtPrivateKeyPath;
    std::string jwtPublicKeyPath;
    std::chrono::seconds jwtAccessTtl;
    std::chrono::seconds jwtRefreshTtl;

    static AppConfig loadFromEnv();
    void log() const;
};

} // namespace monggle
