#include "monggle/app_config.h"
#include "monggle/auth/auth_service.h"
#include "monggle/auth/jwt_service.h"
#include "monggle/follows/follows_service.h"
#include "monggle/posts/posts_service.h"
#include "monggle/posts/snapshot_service.h"
#include "monggle/router/routes.h"

#include <drogon/drogon.h>

#include <memory>

int main() {
    auto cfg = monggle::AppConfig::loadFromEnv();
    cfg.log();

    auto jwtService = std::make_shared<monggle::JwtService>(
        monggle::readPemFile(cfg.jwtPrivateKeyPath),
        monggle::readPemFile(cfg.jwtPublicKeyPath),
        cfg.jwtIssuer,
        cfg.jwtAccessTtl,
        cfg.jwtRefreshTtl);

    auto authService     = std::make_shared<monggle::AuthService>(jwtService);
    auto followsService  = std::make_shared<monggle::FollowsService>();
    auto postsService    = std::make_shared<monggle::PostsService>(followsService);
    auto snapshotService = std::make_shared<monggle::SnapshotService>();

    monggle::configureHealthRoutes();
    monggle::configureAuthRoutes(authService);
    monggle::configurePostsRoutes(authService, postsService);
    monggle::configureSnapshotRoutes(authService, snapshotService);
    monggle::configureFollowsRoutes(authService, followsService);

    drogon::app()
        .createDbClient(
            "mysql",
            cfg.dbHost,
            cfg.dbPort,
            cfg.dbName,
            cfg.dbUser,
            cfg.dbPassword,
            cfg.dbPoolSize,
            "",
            "monggle_db",
            false,
            "utf8mb4")
        .createRedisClient(
            cfg.redisHost,
            cfg.redisPort,
            "monggle_redis",
            "",
            cfg.redisPoolSize,
            true,
            0)
        .addListener(cfg.httpHost, cfg.httpPort)
        .setThreadNum(0)
        .setLogLevel(trantor::Logger::kInfo);

    LOG_INFO << "monggle server listening on " << cfg.httpHost << ":" << cfg.httpPort;
    drogon::app().run();
    return 0;
}
