#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/posts/snapshot_service.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <memory>
#include <string>

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

drogon::HttpResponsePtr snapshotErrorResponse(const SnapshotError& e, const std::string& instance) {
    switch (e.code) {
        case SnapshotError::BadRequest:
            return problemJson(drogon::k400BadRequest, "bad_request", e.detail, instance);
        case SnapshotError::InternalError:
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error", e.detail, instance);
    }
}

} // namespace

void configureSnapshotRoutes(std::shared_ptr<AuthService> authService,
                             std::shared_ptr<SnapshotService> snapshotService) {
    drogon::app().registerHandler(
        "/me/snapshot",
        [authService, snapshotService](const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/snapshot"));
                return;
            }
            auto at = req->getParameter("at");
            if (at.empty()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "at query parameter required (ISO 8601)", "/me/snapshot"));
                return;
            }

            auto result = snapshotService->restoreState(*userId, at);
            if (auto* err = std::get_if<SnapshotError>(&result)) {
                cb(snapshotErrorResponse(*err, "/me/snapshot"));
                return;
            }
            const auto& state = std::get<UserStateAt>(result);
            Json::Value body(Json::objectValue);
            body["user_id"]     = static_cast<Json::Int64>(state.userId);
            body["target_time"] = state.targetTime;
            Json::Value posts(Json::arrayValue);
            for (const auto& p : state.posts) {
                Json::Value j(Json::objectValue);
                j["id"]            = static_cast<Json::Int64>(p.id);
                j["title"]         = p.title;
                j["body"]          = p.body;
                j["visibility"]    = p.visibility;
                j["deleted"]       = p.deleted;
                j["last_event_id"] = static_cast<Json::Int64>(p.lastEventId);
                posts.append(j);
            }
            body["posts"] = posts;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});
}

} // namespace monggle
