#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/follows/follows_service.h"
#include "monggle/notifications/notifications_service.h"

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

drogon::HttpResponsePtr followErrorResponse(const FollowError& e, const std::string& instance) {
    switch (e.code) {
        case FollowError::NotFound:
            return problemJson(drogon::k404NotFound, "not_found", e.detail, instance);
        case FollowError::AlreadyFollowing:
            return problemJson(drogon::k409Conflict, "already_following", e.detail, instance);
        case FollowError::SelfFollow:
            return problemJson(drogon::k400BadRequest, "self_follow", e.detail, instance);
        case FollowError::BadRequest:
            return problemJson(drogon::k400BadRequest, "bad_request", e.detail, instance);
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error", e.detail, instance);
    }
}

std::optional<std::int64_t> requireAuth(
    const std::shared_ptr<AuthService>& authService,
    const drogon::HttpRequestPtr& req,
    const std::function<void(const drogon::HttpResponsePtr&)>& cb,
    const std::string& instance) {
    auto uid = authService->verifyAccess(std::string(req->getHeader("Authorization")));
    if (!uid) {
        cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                       "valid Bearer access token required", instance));
    }
    return uid;
}

std::int64_t parseUserIdFromPath(const std::string& path) {
    // /users/{id}/follow 또는 /users/{id}
    auto rest = path.substr(std::string("/users/").size());
    auto slash = rest.find('/');
    return std::stoll(rest.substr(0, slash));
}

Json::Value userBriefToJson(const UserBrief& u) {
    Json::Value j(Json::objectValue);
    j["id"]           = static_cast<Json::Int64>(u.id);
    j["email"]        = u.email;
    j["display_name"] = u.displayName;
    return j;
}

} // namespace

void configureFollowsRoutes(std::shared_ptr<AuthService> authService,
                            std::shared_ptr<FollowsService> followsService,
                            std::shared_ptr<NotificationsService> notif) {
    // POST /users/{id}/follow, DELETE /users/{id}/follow
    drogon::app().registerHandlerViaRegex(
        "^/users/([0-9]+)/follow$",
        [authService, followsService, notif](const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            auto userId = requireAuth(authService, req, cb, path);
            if (!userId) return;

            std::int64_t targetId = 0;
            try { targetId = parseUserIdFromPath(path); }
            catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid user id", path));
                return;
            }

            if (req->getMethod() == drogon::Post) {
                auto r = followsService->follow(*userId, targetId);
                if (auto* e = std::get_if<FollowError>(&r)) {
                    cb(followErrorResponse(*e, path));
                } else {
                    if (notif) {
                        notif->enqueue(targetId, "follow", *userId, std::nullopt,
                                       "새 팔로워가 생겼어요");
                    }
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    cb(resp);
                }
                return;
            }
            if (req->getMethod() == drogon::Delete) {
                auto r = followsService->unfollow(*userId, targetId);
                if (auto* e = std::get_if<FollowError>(&r)) {
                    cb(followErrorResponse(*e, path));
                } else {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    cb(resp);
                }
                return;
            }
            cb(problemJson(drogon::k405MethodNotAllowed, "method_not_allowed",
                           "use POST/DELETE", path));
        },
        {drogon::Post, drogon::Delete});

    // GET /me/followers
    drogon::app().registerHandler(
        "/me/followers",
        [authService, followsService](const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = requireAuth(authService, req, cb, "/me/followers");
            if (!userId) return;
            auto r = followsService->listFollowers(*userId, 100);
            if (auto* e = std::get_if<FollowError>(&r)) {
                cb(followErrorResponse(*e, "/me/followers"));
                return;
            }
            const auto& list = std::get<std::vector<UserBrief>>(r);
            Json::Value body(Json::objectValue);
            Json::Value items(Json::arrayValue);
            for (const auto& u : list) items.append(userBriefToJson(u));
            body["items"] = items;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});

    // GET /me/following
    drogon::app().registerHandler(
        "/me/following",
        [authService, followsService](const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = requireAuth(authService, req, cb, "/me/following");
            if (!userId) return;
            auto r = followsService->listFollowing(*userId, 100);
            if (auto* e = std::get_if<FollowError>(&r)) {
                cb(followErrorResponse(*e, "/me/following"));
                return;
            }
            const auto& list = std::get<std::vector<UserBrief>>(r);
            Json::Value body(Json::objectValue);
            Json::Value items(Json::arrayValue);
            for (const auto& u : list) items.append(userBriefToJson(u));
            body["items"] = items;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});

    // GET /me/feed
    drogon::app().registerHandler(
        "/me/feed",
        [authService, followsService](const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = requireAuth(authService, req, cb, "/me/feed");
            if (!userId) return;

            std::int64_t cursor = 0;
            int limit = 30;
            if (auto c = req->getParameter("cursor"); !c.empty()) {
                try { cursor = std::stoll(c); } catch (...) {}
            }
            if (auto l = req->getParameter("limit"); !l.empty()) {
                try { limit = std::stoi(l); } catch (...) {}
            }

            auto r = followsService->feed(*userId, cursor, limit);
            if (auto* e = std::get_if<FollowError>(&r)) {
                cb(followErrorResponse(*e, "/me/feed"));
                return;
            }
            const auto& page = std::get<FollowsService::FeedPage>(r);
            Json::Value body(Json::objectValue);
            Json::Value items(Json::arrayValue);
            for (const auto& it : page.items) {
                Json::Value j(Json::objectValue);
                j["id"]              = static_cast<Json::Int64>(it.id);
                j["user_id"]         = static_cast<Json::Int64>(it.userId);
                j["author_name"]     = it.authorDisplayName;
                j["title"]           = it.title;
                j["body"]            = it.body;
                j["visibility"]      = it.visibility;
                j["download_policy"] = it.downloadPolicy;
                j["created_at"]      = it.createdAt;
                j["updated_at"]      = it.updatedAt;
                items.append(j);
            }
            body["items"] = items;
            body["next_cursor"] = page.hasNext
                                      ? static_cast<Json::Int64>(page.nextCursor)
                                      : Json::Value(Json::nullValue);
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});
}

} // namespace monggle
