#include "monggle/router/routes.h"
#include "monggle/entry/entry_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>

#include <json/json.h>

#include <memory>

namespace monggle {

namespace {

void writeJson(const std::function<void(const drogon::HttpResponsePtr&)>& callback,
               drogon::HttpStatusCode status,
               const Json::Value& body) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(status);
    callback(resp);
}

} // namespace

void configureRoutes(std::shared_ptr<EntryService> entryService) {
    drogon::app().registerHandler(
        "/api/entry/create",
        [entryService](const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            std::string token{req->getHeader("Authorization")};
            std::string body{req->body()};

            entryService->createEntry(token, body);

            Json::Value res;
            res["status"] = "queued";
            writeJson(callback, drogon::k202Accepted, res);
        },
        {drogon::Post});

    // 가벼운 헬스체크 (의존성 미점검)
    drogon::app().registerHandler(
        "/healthz",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            Json::Value res;
            res["status"] = "ok";
            writeJson(callback, drogon::k200OK, res);
        },
        {drogon::Get});

    // DB ping 으로 readiness 점검 (Redis는 후속 단계에서 추가)
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
                        res["db"] = "ok";
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                        resp->setStatusCode(drogon::k200OK);
                        (*cb)(resp);
                    },
                    [cb](const drogon::orm::DrogonDbException& e) {
                        Json::Value res;
                        res["db"] = std::string("error: ") + e.base().what();
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                        resp->setStatusCode(drogon::k503ServiceUnavailable);
                        (*cb)(resp);
                    });
            } catch (const std::exception& e) {
                Json::Value res;
                res["db"] = std::string("error: ") + e.what();
                auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                resp->setStatusCode(drogon::k503ServiceUnavailable);
                (*cb)(resp);
            }
        },
        {drogon::Get});

    // TODO: snapshot restore, embedding search, download tasks, friend feed, media uploads
}

} // namespace monggle
