#include "monggle/router/routes.h"
#include "monggle/entry/entry_service.h"

#include <drogon/drogon.h>

namespace monggle {

void configureRoutes(std::shared_ptr<EntryService> entryService) {
    drogon::app().registerHandler(
        "/api/entry/create",
        [entryService](const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            std::string token{req->getHeader("Authorization")};
            std::string body{req->body()};

            entryService->createEntry(token, body);

            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k202Accepted);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"status":"queued"})");
            callback(resp);
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/healthz",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"status":"ok"})");
            callback(resp);
        },
        {drogon::Get});

    // TODO: snapshot restore, embedding search, download tasks, friend feed, media uploads
}

} // namespace monggle
