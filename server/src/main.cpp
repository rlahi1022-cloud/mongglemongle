#include "monggle/app_config.h"
#include "monggle/ai/ai_hub_client.h"
#include "monggle/auth/auth_service.h"
#include "monggle/auth/jwt_service.h"
#include "monggle/blocks/blocks_service.h"
#include "monggle/cache/layered_cache.h"
#include "monggle/cache/redis_cache.h"
#include "monggle/cache/ttl_cache.h"
#include "monggle/comments/comments_service.h"
#include "monggle/follows/follows_service.h"
#include "monggle/media/local_fs_storage.h"
#include "monggle/media/media_queue.h"
#include "monggle/media/media_service.h"
#include "monggle/media/minio_storage.h"
#include "monggle/middleware/cors.h"
#include "monggle/middleware/rate_limiter.h"
#include "monggle/middleware/request_log.h"
#include "monggle/notifications/notifications_service.h"
#include "monggle/posts/posts_service.h"
#include "monggle/posts/snapshot_service.h"
#include "monggle/posts/snapshot_worker.h"
#include "monggle/profile/profile_service.h"
#include "monggle/router/routes.h"

#include <drogon/drogon.h>
#include <sw/redis++/redis++.h>

#include <iostream>
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
    // AI Hub는 lazy로 — startup healthy 체크는 drogon event loop이 아직 안 돌아서
    // sync HttpClient::sendRequest가 timeout 적용 없이 hang하는 원인이었음.
    // 실제 호출 경로(embed/compare/draft)는 try-catch로 감싸여 있어 다운 시 자동 fallback.
    std::shared_ptr<monggle::AiHubClient> aiHubClient;
    if (!cfg.aiHubBaseUrl.empty()) {
        aiHubClient = std::make_shared<monggle::AiHubClient>(cfg.aiHubBaseUrl, cfg.aiHubTimeout);
        LOG_INFO << "[ai-hub] configured at " << cfg.aiHubBaseUrl
                 << " (lazy — checked on first request)";
    }
    auto postsService    = std::make_shared<monggle::PostsService>(followsService, blocksService, aiHubClient);
    auto snapshotService = std::make_shared<monggle::SnapshotService>();
    // 미디어 스토리지 백엔드 선택. MONGGLE_S3_ENDPOINT 가 설정되어 있고 MinIO가 응답하면
    // S3 호환 백엔드를 사용. 그 외에는 로컬 파일시스템.
    std::shared_ptr<monggle::IMediaStorage> mediaStorage;
    if (!cfg.s3Endpoint.empty() && !cfg.s3Bucket.empty()
        && !cfg.s3AccessKey.empty() && !cfg.s3SecretKey.empty()) {
        monggle::MinioStorage::Options mopts;
        // s3Endpoint가 "http://host:port" 또는 "host:port" 형식이라 가정. minio-cpp는 host:port를 받음.
        std::string ep = cfg.s3Endpoint;
        bool secure = false;
        if (ep.rfind("https://", 0) == 0) { secure = true; ep = ep.substr(8); }
        else if (ep.rfind("http://", 0) == 0) { ep = ep.substr(7); }
        mopts.endpoint  = ep;
        mopts.accessKey = cfg.s3AccessKey;
        mopts.secretKey = cfg.s3SecretKey;
        mopts.bucket    = cfg.s3Bucket;
        mopts.region    = cfg.s3Region.empty() ? "us-east-1" : cfg.s3Region;
        mopts.secure    = secure;
        mopts.workDir   = cfg.mediaStorageRoot;
        auto minio = std::make_shared<monggle::MinioStorage>(mopts);
        if (minio->healthy()) {
            LOG_INFO << "[media] storage backend: minio @ " << ep
                     << " bucket=" << cfg.s3Bucket;
            mediaStorage = minio;
        } else {
            LOG_WARN << "[media] minio unhealthy at startup — falling back to local-fs";
        }
    }
    if (!mediaStorage) {
        mediaStorage = std::make_shared<monggle::LocalFsStorage>(cfg.mediaStorageRoot);
        LOG_INFO << "[media] storage backend: local-fs root=" << cfg.mediaStorageRoot;
    }
    auto mediaService    = std::make_shared<monggle::MediaService>(cfg.mediaStorageRoot, mediaStorage, followsService, blocksService);
    auto profileService  = std::make_shared<monggle::ProfileService>(cfg.mediaStorageRoot);
    auto notifService    = std::make_shared<monggle::NotificationsService>();
    auto commentsService = std::make_shared<monggle::CommentsService>(followsService, notifService);

    // L1(in-process) + L2(Redis) 피드 캐시. Redis 다운 시 L1만으로 graceful degrade.
    // Rate Limiter도 같은 Redis를 공유 (별도 connection pool, 트래픽 격리는 X — 단순함 우선).
    {
        monggle::RedisCache::Options ropts;
        ropts.host = cfg.redisHost;
        ropts.port = cfg.redisPort;
        ropts.poolSize = cfg.redisPoolSize;
        auto l2 = std::make_shared<monggle::RedisCache>(ropts);
        if (l2->healthy()) {
            LOG_INFO << "[redis] connected at " << cfg.redisHost << ":" << cfg.redisPort
                     << " (pool=" << cfg.redisPoolSize << ")";
        } else {
            LOG_WARN << "[redis] unavailable at startup; feed cache running L1-only";
        }
        monggle::LayeredCache::initFeed(std::make_shared<monggle::TtlCache>(), l2);

        // Rate Limiter용 별도 Redis (작은 풀). 캐시와 격리해서 캐시 트래픽 폭주가
        // ratelimit 평가를 막지 않도록.
        try {
            sw::redis::ConnectionOptions co;
            co.host = cfg.redisHost;
            co.port = cfg.redisPort;
            co.connect_timeout = std::chrono::milliseconds(300);
            co.socket_timeout  = std::chrono::milliseconds(300);
            sw::redis::ConnectionPoolOptions po;
            po.size = 2;
            auto rlRedis = std::make_shared<sw::redis::Redis>(co, po);
            monggle::RateLimiter::instance().setRedis(rlRedis);
            if (monggle::RateLimiter::instance().redisHealthy()) {
                LOG_INFO << "[ratelimit] redis backend enabled";
            } else {
                LOG_WARN << "[ratelimit] redis backend not healthy — using in-memory";
            }

            // Refresh token allowlist도 같은 Redis 핸들 공유 — 작은 트래픽이라 분리 불필요.
            authService->setRedis(rlRedis);
            if (authService->redisHealthy()) {
                LOG_INFO << "[auth] refresh-token redis allowlist enabled";
            } else {
                LOG_WARN << "[auth] refresh-token redis allowlist not healthy — DB-only";
            }

            // 미디어 처리 큐 (Redis Stream). 영상 인코딩/썸네일 같은 무거운 작업을 비동기로
            // 분리하기 위한 인프라. 실제 sync 처리 분기는 MediaService 내부에서 결정.
            static monggle::MediaQueue mediaQueue;
            mediaQueue.setRedis(rlRedis, "media:jobs", "media-workers", "monggle-c1");
            if (mediaQueue.healthy()) {
                LOG_INFO << "[media-queue] redis stream backend ready (key=media:jobs)";
            }
        } catch (const std::exception& e) {
            LOG_WARN << "[ratelimit] redis backend init failed: " << e.what()
                     << " — using in-memory";
        }
    }

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

    // 스냅샷 워커: drogon이 listener를 시작한 후(=DB 클라이언트 ready) 사이클을 시작.
    // 60s 후 첫 사이클을 돌리도록 timer로 트리거하면 드로곤 event loop이 DB를 잡은 뒤 안전.
    monggle::SnapshotWorker snapshotWorker;
    drogon::app().getLoop()->runAfter(60.0, [&snapshotWorker]() {
        LOG_INFO << "[snapshot-worker] starting (interval=1h, lookback=24h)";
        snapshotWorker.start();
    });

    drogon::app().run();
    snapshotWorker.stop();
    return 0;
}
