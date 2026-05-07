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
    return c;
}

void AppConfig::log() const {
    LOG_INFO << "[AppConfig] http=" << httpHost << ":" << httpPort
             << " db=" << dbUser << "@" << dbHost << ":" << dbPort << "/" << dbName
             << " (pool=" << dbPoolSize << ")"
             << " redis=" << redisHost << ":" << redisPort
             << " (pool=" << redisPoolSize << ")";
}

} // namespace monggle
