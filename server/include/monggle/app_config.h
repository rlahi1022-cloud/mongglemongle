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

    std::string mediaStorageRoot;   // 로컬 미디어 저장 루트

    // AI 허브 (Python FastAPI). 비어있으면 의미 검색 비활성.
    std::string aiHubBaseUrl;
    std::chrono::milliseconds aiHubTimeout;

    // S3 (MinIO/AWS) — MVP는 사용 안 하지만 후속 마이그레이션 위해 미리 둠.
    std::string s3Endpoint;        // MinIO: http://127.0.0.1:9000, AWS: ""
    std::string s3Bucket;
    std::string s3AccessKey;
    std::string s3SecretKey;
    std::string s3Region;

    static AppConfig loadFromEnv();
    void log() const;
};

} // namespace monggle
