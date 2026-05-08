#include "monggle/router/routes.h"
#include "monggle/cache/layered_cache.h"
#include "monggle/metrics/metrics.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>

#include <json/json.h>

#include <memory>

namespace monggle {

void configureHealthRoutes() {
    auto& m = Metrics::instance();
    m.describeCounter("http_requests_total", "Total HTTP requests by method, normalized path, and status");
    m.describeHistogram("http_request_duration_seconds", "HTTP request duration in seconds");
    m.describeCounter("cache_hits_total",   "Layered cache hits broken down by layer (l1|l2)");
    m.describeCounter("cache_misses_total", "Layered cache misses (neither L1 nor L2 had the key)");
    m.describeCounter("ratelimit_denied_total", "Rate-limit decisions that returned deny, by policy group");
    m.describeGauge("redis_up",  "1 if a Redis backend (cache or auth or ratelimit) is healthy");

    drogon::app().registerHandler(
        "/metrics",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            // Update gauges that are cheap to read on demand.
            Metrics::instance().setGauge("redis_up", {{"role", "feed_l2"}},
                LayeredCache::feed().l2Healthy() ? 1.0 : 0.0);

            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody(Metrics::instance().render());
            // Prometheus exposition is text/plain; charset 명시.
            resp->setContentTypeString("text/plain; version=0.0.4; charset=utf-8");
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/healthz",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            Json::Value res;
            res["status"] = "ok";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        },
        {drogon::Get});

    // DB ping + Redis 상태. Redis는 redis-plus-plus(L2 LayeredCache)에 위임 —
    // 살아있으면 "ok", 끊겨있으면 "down" (서비스는 L1만으로 계속 동작하므로 503 트리거 X).
    drogon::app().registerHandler(
        "/readyz",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto cb = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const std::string redisStatus = LayeredCache::feed().l2Healthy() ? "ok" : "down";
            try {
                auto db = drogon::app().getDbClient("monggle_db");
                db->execSqlAsync(
                    "SELECT 1",
                    [cb, redisStatus](const drogon::orm::Result&) {
                        Json::Value res;
                        res["db"]    = "ok";
                        res["redis"] = redisStatus;
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                        resp->setStatusCode(drogon::k200OK);
                        (*cb)(resp);
                    },
                    [cb, redisStatus](const drogon::orm::DrogonDbException& e) {
                        Json::Value res;
                        res["db"]    = std::string("error: ") + e.base().what();
                        res["redis"] = redisStatus;
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                        resp->setStatusCode(drogon::k503ServiceUnavailable);
                        (*cb)(resp);
                    });
            } catch (const std::exception& e) {
                Json::Value res;
                res["db"]    = std::string("error: ") + e.what();
                res["redis"] = redisStatus;
                auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                resp->setStatusCode(drogon::k503ServiceUnavailable);
                (*cb)(resp);
            }
        },
        {drogon::Get});
}

} // namespace monggle
