#include "monggle/app_config.h"

#include <trantor/utils/Logger.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace monggle {
namespace {

std::string envOr(const char* name, const std::string& fallback) {
    if (const char* v = std::getenv(name); v && *v) return v;
    return fallback;
}

std::uint16_t envOrU16(const char* name, std::uint16_t fallback) {
    if (const char* v = std::getenv(name); v && *v) {
        return static_cast<std::uint16_t>(std::stoi(v));
    }
    return fallback;
}

std::size_t envOrSize(const char* name, std::size_t fallback) {
    if (const char* v = std::getenv(name); v && *v) {
        return static_cast<std::size_t>(std::stoul(v));
    }
    return fallback;
}

std::int64_t envOrI64(const char* name, std::int64_t fallback) {
    if (const char* v = std::getenv(name); v && *v) {
        return static_cast<std::int64_t>(std::stoll(v));
    }
    return fallback;
}

} // namespace

AppConfig AppConfig::loadFromEnv() {
    AppConfig c;
    c.httpHost       = envOr   ("MONGGLE_HTTP_HOST",      "0.0.0.0");
    c.httpPort       = envOrU16("MONGGLE_HTTP_PORT",      8080);

    c.dbHost         = envOr   ("MONGGLE_DB_HOST",        "127.0.0.1");
    c.dbPort         = envOrU16("MONGGLE_DB_PORT",        3306);
    c.dbName         = envOr   ("MONGGLE_DB_NAME",        "monggle");
    c.dbUser         = envOr   ("MONGGLE_DB_USER",        "monggle");
    c.dbPassword     = envOr   ("MONGGLE_DB_PASSWORD",    "monggle_dev");
    c.dbPoolSize     = envOrSize("MONGGLE_DB_POOL_SIZE",  8);

    c.redisHost      = envOr   ("MONGGLE_REDIS_HOST",     "127.0.0.1");
    c.redisPort      = envOrU16("MONGGLE_REDIS_PORT",     6379);
    c.redisPoolSize  = envOrSize("MONGGLE_REDIS_POOL_SIZE", 4);

    c.jwtIssuer         = envOr("MONGGLE_JWT_ISSUER",            "monggle.local");
    c.jwtPrivateKeyPath = envOr("MONGGLE_JWT_PRIVATE_KEY_PATH",  "keys/dev_jwt_private.pem");
    c.jwtPublicKeyPath  = envOr("MONGGLE_JWT_PUBLIC_KEY_PATH",   "keys/dev_jwt_public.pem");
    c.jwtAccessTtl      = std::chrono::seconds(envOrI64("MONGGLE_JWT_ACCESS_TTL_SECONDS",   900));    // 15m
    c.jwtRefreshTtl     = std::chrono::seconds(envOrI64("MONGGLE_JWT_REFRESH_TTL_SECONDS",  60 * 60 * 24 * 14)); // 14d

    c.mediaStorageRoot  = envOr("MONGGLE_MEDIA_STORAGE_ROOT", "media");

    c.aiHubBaseUrl      = envOr("MONGGLE_AI_HUB_BASE_URL", "http://127.0.0.1:9100");
    c.aiHubTimeout      = std::chrono::milliseconds(envOrI64("MONGGLE_AI_HUB_TIMEOUT_MS", 5000));

    c.s3Endpoint        = envOr("MONGGLE_S3_ENDPOINT",   "http://127.0.0.1:9002");
    c.s3Bucket          = envOr("MONGGLE_S3_BUCKET",     "monggle-media");
    c.s3AccessKey       = envOr("MONGGLE_S3_ACCESS_KEY", "monggle_admin");
    c.s3SecretKey       = envOr("MONGGLE_S3_SECRET_KEY", "monggle_dev_secret");
    c.s3Region          = envOr("MONGGLE_S3_REGION",     "us-east-1");
    return c;
}

void AppConfig::log() const {
    LOG_INFO << "[AppConfig] http=" << httpHost << ":" << httpPort
             << " db=" << dbUser << "@" << dbHost << ":" << dbPort << "/" << dbName
             << " (pool=" << dbPoolSize << ")"
             << " redis=" << redisHost << ":" << redisPort
             << " (pool=" << redisPoolSize << ")"
             << " jwt_issuer=" << jwtIssuer
             << " jwt_access_ttl=" << jwtAccessTtl.count() << "s"
             << " jwt_refresh_ttl=" << jwtRefreshTtl.count() << "s"
             << " media_root=" << mediaStorageRoot;
}

} // namespace monggle
