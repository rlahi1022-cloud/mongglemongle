#include "monggle/app_config.h"
#include "monggle/auth/auth_service.h"
#include "monggle/event/event_bus.h"
#include "monggle/entry/entry_service.h"
#include "monggle/router/routes.h"

#include <drogon/drogon.h>

#include <memory>

int main() {
    auto cfg = monggle::AppConfig::loadFromEnv();
    cfg.log();

    auto authService  = std::make_shared<monggle::AuthService>();
    auto eventBus     = std::make_shared<monggle::EventBus>();
    auto entryService = std::make_shared<monggle::EntryService>(*authService, *eventBus);

    monggle::configureRoutes(entryService);

    drogon::app()
        .createDbClient(
            "mysql",
            cfg.dbHost,
            cfg.dbPort,
            cfg.dbName,
            cfg.dbUser,
            cfg.dbPassword,
            cfg.dbPoolSize,
            "",                 // filename (sqlite only)
            "monggle_db",       // client name
            false,              // auto batch (postgres only)
            "utf8mb4")
        .createRedisClient(
            cfg.redisHost,
            cfg.redisPort,
            "monggle_redis",    // client name
            "",                 // password
            cfg.redisPoolSize,
            true,               // is fast
            0)                  // db index
        .addListener(cfg.httpHost, cfg.httpPort)
        .setThreadNum(0)
        .setLogLevel(trantor::Logger::kInfo);

    LOG_INFO << "monggle server listening on " << cfg.httpHost << ":" << cfg.httpPort;
    drogon::app().run();
    return 0;
}
