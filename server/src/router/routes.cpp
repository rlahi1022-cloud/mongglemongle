#include "monggle/router/routes.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>

#include <json/json.h>

#include <memory>

namespace monggle {

void configureHealthRoutes() {
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

    // DB ping. Redis ping은 Drogon 1.8.7 (Ubuntu 24.04) RedisClient의
    // 비호환으로 segfault → Redis 도입 안정화 시 재추가 (TODO).
    drogon::app().registerHandler(
        "/readyz",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto cb = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            try {
                auto db = drogon::app().getDbClient("monggle_db");
                db->execSqlAsync(
                    "SELECT 1",
                    [cb](const drogon::orm::Result&) {
                        Json::Value res;
                        res["db"]    = "ok";
                        res["redis"] = "skipped";
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                        resp->setStatusCode(drogon::k200OK);
                        (*cb)(resp);
                    },
                    [cb](const drogon::orm::DrogonDbException& e) {
                        Json::Value res;
                        res["db"]    = std::string("error: ") + e.base().what();
                        res["redis"] = "skipped";
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                        resp->setStatusCode(drogon::k503ServiceUnavailable);
                        (*cb)(resp);
                    });
            } catch (const std::exception& e) {
                Json::Value res;
                res["db"]    = std::string("error: ") + e.what();
                res["redis"] = "skipped";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                resp->setStatusCode(drogon::k503ServiceUnavailable);
                (*cb)(resp);
            }
        },
        {drogon::Get});
}

} // namespace monggle
