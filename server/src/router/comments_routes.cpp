#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/comments/comments_service.h"

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

drogon::HttpResponsePtr commentErrorResponse(const CommentError& e, const std::string& instance) {
    switch (e.code) {
        case CommentError::NotFound:
            return problemJson(drogon::k404NotFound, "not_found", e.detail, instance);
        case CommentError::Forbidden:
            return problemJson(drogon::k403Forbidden, "forbidden", e.detail, instance);
        case CommentError::BadRequest:
            return problemJson(drogon::k400BadRequest, "bad_request", e.detail, instance);
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error", e.detail, instance);
    }
}

Json::Value commentToJson(const Comment& c) {
    Json::Value j(Json::objectValue);
    j["id"]          = static_cast<Json::Int64>(c.id);
    j["post_id"]     = static_cast<Json::Int64>(c.postId);
    j["user_id"]     = static_cast<Json::Int64>(c.userId);
    j["body"]        = c.body;
    j["author_name"] = c.authorName;
    j["created_at"]  = c.createdAt;
    return j;
}

std::int64_t parseSegment(const std::string& path, const std::string& prefix) {
    auto rest = path.substr(prefix.size());
    auto slash = rest.find('/');
    return std::stoll(rest.substr(0, slash));
}

} // namespace

void configureCommentsRoutes(std::shared_ptr<AuthService> authService,
                             std::shared_ptr<CommentsService> commentsService) {
    // GET / POST /posts/{id}/comments
    drogon::app().registerHandlerViaRegex(
        "^/posts/([0-9]+)/comments$",
        [authService, commentsService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            std::int64_t postId = 0;
            try { postId = parseSegment(path, "/posts/"); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid post id", path));
                return;
            }

            auto auth = authService->verifyAccess(std::string(req->getHeader("Authorization")));

            if (req->getMethod() == drogon::Get) {
                std::int64_t viewerId = auth.value_or(-1);
                auto r = commentsService->listForPost(viewerId, postId);
                if (auto* e = std::get_if<CommentError>(&r)) {
                    cb(commentErrorResponse(*e, path));
                    return;
                }
                Json::Value body(Json::objectValue);
                Json::Value arr(Json::arrayValue);
                for (const auto& c : std::get<std::vector<Comment>>(r)) arr.append(commentToJson(c));
                body["items"] = arr;
                cb(drogon::HttpResponse::newHttpJsonResponse(body));
                return;
            }

            // POST — 인증 필요
            if (!auth) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", path));
                return;
            }
            auto json = req->getJsonObject();
            if (!json || !json->isMember("body") || !(*json)["body"].isString()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "body (string) required", path));
                return;
            }
            std::string body = (*json)["body"].asString();

            auto r = commentsService->create(*auth, postId, body);
            if (auto* e = std::get_if<CommentError>(&r)) {
                cb(commentErrorResponse(*e, path));
                return;
            }
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                commentToJson(std::get<Comment>(r)));
            resp->setStatusCode(drogon::k201Created);
            cb(resp);
        },
        {drogon::Get, drogon::Post});

    // DELETE /comments/{id}
    drogon::app().registerHandlerViaRegex(
        "^/comments/([0-9]+)$",
        [authService, commentsService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", path));
                return;
            }
            std::int64_t commentId = 0;
            try { commentId = parseSegment(path, "/comments/"); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid id", path));
                return;
            }
            auto r = commentsService->remove(*userId, commentId);
            if (auto* e = std::get_if<CommentError>(&r)) {
                cb(commentErrorResponse(*e, path));
            } else {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                cb(resp);
            }
        },
        {drogon::Delete});
}

} // namespace monggle
