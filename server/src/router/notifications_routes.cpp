#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/notifications/notifications_service.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <memory>

namespace monggle {

namespace {

drogon::HttpResponsePtr problemJson(drogon::HttpStatusCode status,
                                    const std::string& title,
                                    const std::string& detail,
                                    const std::string& instance) {
    Json::Value body;
    body["type"]     = "https://monggle.local/errors/" + title;
    body["title"]    = title;
    body["status"]   = static_cast<int>(status);
    body["detail"]   = detail;
    body["instance"] = instance;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(status);
    return resp;
}

Json::Value notifToJson(const NotificationItem& n) {
    Json::Value j(Json::objectValue);
    j["id"]         = static_cast<Json::Int64>(n.id);
    j["kind"]       = n.kind;
    j["actor_id"]   = n.actorId   ? Json::Value(static_cast<Json::Int64>(*n.actorId))   : Json::Value(Json::nullValue);
    j["target_id"]  = n.targetId  ? Json::Value(static_cast<Json::Int64>(*n.targetId))  : Json::Value(Json::nullValue);
    j["body"]       = n.body;
    j["is_read"]    = n.isRead;
    j["created_at"] = n.createdAt;
    j["actor_name"] = n.actorName;
    return j;
}

} // namespace

void configureNotificationsRoutes(std::shared_ptr<AuthService> authService,
                                  std::shared_ptr<NotificationsService> notif) {
    // GET /me/notifications
    drogon::app().registerHandler(
        "/me/notifications",
        [authService, notif](const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/notifications"));
                return;
            }
            auto items = notif->recent(*userId, 30);
            int unread = notif->unreadCount(*userId);
            Json::Value body(Json::objectValue);
            Json::Value arr(Json::arrayValue);
            for (const auto& n : items) arr.append(notifToJson(n));
            body["items"]  = arr;
            body["unread"] = unread;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});

    // POST /me/notifications/read — 모두 읽음 처리
    drogon::app().registerHandler(
        "/me/notifications/read",
        [authService, notif](const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/notifications/read"));
                return;
            }
            notif->markAllRead(*userId);
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        {drogon::Post});
}

} // namespace monggle
