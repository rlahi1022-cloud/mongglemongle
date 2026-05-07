#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/blocks/blocks_service.h"

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

drogon::HttpResponsePtr blockErrorResponse(const BlockError& e, const std::string& instance) {
    switch (e.code) {
        case BlockError::NotFound:
            return problemJson(drogon::k404NotFound, "not_found", e.detail, instance);
        case BlockError::SelfBlock:
            return problemJson(drogon::k400BadRequest, "self_block", e.detail, instance);
        case BlockError::AlreadyBlocked:
            return problemJson(drogon::k409Conflict, "already_blocked", e.detail, instance);
        case BlockError::BadRequest:
            return problemJson(drogon::k400BadRequest, "bad_request", e.detail, instance);
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error", e.detail, instance);
    }
}

std::int64_t parseUserId(const std::string& path) {
    auto rest = path.substr(std::string("/users/").size());
    auto slash = rest.find('/');
    return std::stoll(rest.substr(0, slash));
}

} // namespace

void configureBlocksRoutes(std::shared_ptr<AuthService> authService,
                           std::shared_ptr<BlocksService> blocksService) {
    // POST/DELETE /users/{id}/block
    drogon::app().registerHandlerViaRegex(
        "^/users/([0-9]+)/block$",
        [authService, blocksService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", path));
                return;
            }
            std::int64_t targetId = 0;
            try { targetId = parseUserId(path); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid user id", path));
                return;
            }
            if (req->getMethod() == drogon::Post) {
                auto r = blocksService->block(*userId, targetId);
                if (auto* e = std::get_if<BlockError>(&r)) {
                    cb(blockErrorResponse(*e, path));
                } else {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    cb(resp);
                }
                return;
            }
            // DELETE
            auto r = blocksService->unblock(*userId, targetId);
            if (auto* e = std::get_if<BlockError>(&r)) {
                cb(blockErrorResponse(*e, path));
            } else {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                cb(resp);
            }
        },
        {drogon::Post, drogon::Delete});

    // GET /me/blocks
    drogon::app().registerHandler(
        "/me/blocks",
        [authService, blocksService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/blocks"));
                return;
            }
            auto r = blocksService->list(*userId, 100);
            if (auto* e = std::get_if<BlockError>(&r)) {
                cb(blockErrorResponse(*e, "/me/blocks"));
                return;
            }
            const auto& list = std::get<std::vector<BlockedUser>>(r);
            Json::Value body(Json::objectValue);
            Json::Value items(Json::arrayValue);
            for (const auto& u : list) {
                Json::Value j(Json::objectValue);
                j["id"]           = static_cast<Json::Int64>(u.id);
                j["email"]        = u.email;
                j["display_name"] = u.displayName;
                j["blocked_at"]   = u.blockedAt;
                items.append(j);
            }
            body["items"] = items;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});
}

} // namespace monggle
