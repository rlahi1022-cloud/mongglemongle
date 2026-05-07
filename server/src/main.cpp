#include "monggle/app_config.h"
#include "monggle/auth/auth_service.h"
#include "monggle/auth/jwt_service.h"
#include "monggle/blocks/blocks_service.h"
#include "monggle/comments/comments_service.h"
#include "monggle/follows/follows_service.h"
#include "monggle/media/media_service.h"
#include "monggle/middleware/cors.h"
#include "monggle/middleware/rate_limiter.h"
#include "monggle/middleware/request_log.h"
#include "monggle/notifications/notifications_service.h"
#include "monggle/posts/posts_service.h"
#include "monggle/posts/snapshot_service.h"
#include "monggle/profile/profile_service.h"
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
    auto blocksService   = std::make_shared<monggle::BlocksService>();
    auto postsService    = std::make_shared<monggle::PostsService>(followsService, blocksService);
    auto snapshotService = std::make_shared<monggle::SnapshotService>();
    auto mediaService    = std::make_shared<monggle::MediaService>(cfg.mediaStorageRoot, followsService, blocksService);
    auto profileService  = std::make_shared<monggle::ProfileService>(cfg.mediaStorageRoot);
    auto notifService    = std::make_shared<monggle::NotificationsService>();
    auto commentsService = std::make_shared<monggle::CommentsService>(followsService, notifService);

    // CORS는 라우트 등록 전에 설치
    monggle::installCors(monggle::defaultDevCors());
    // 모든 응답에 액세스 로그 한 줄
    monggle::installRequestLog();
    // RateLimiter 싱글톤 워밍 (정책 등록)
    (void) monggle::RateLimiter::instance();

    monggle::configureHealthRoutes();
    monggle::configureAuthRoutes(authService);
    monggle::configurePostsRoutes(authService, postsService);
    monggle::configureSnapshotRoutes(authService, snapshotService);
    monggle::configureFollowsRoutes(authService, followsService, notifService);
    monggle::configureMediaRoutes(authService, followsService, mediaService, cfg.mediaStorageRoot);
    monggle::configureProfileRoutes(authService, profileService);
    monggle::configureCommentsRoutes(authService, commentsService);
    monggle::configureNotificationsRoutes(authService, notifService);
    monggle::configureBlocksRoutes(authService, blocksService);

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
        // NOTE: Drogon 1.8.7 (Ubuntu 24.04) RedisClient가 segfault 발생.
        // L2 캐시·readyz Redis ping은 RedisClient 안정화 또는 hiredis
        // 직접 사용으로 후속 작업.
        .addListener(cfg.httpHost, cfg.httpPort)
        .setThreadNum(0)
        .setLogLevel(trantor::Logger::kInfo);

    LOG_INFO << "monggle server listening on " << cfg.httpHost << ":" << cfg.httpPort;
    drogon::app().run();
    return 0;
}
