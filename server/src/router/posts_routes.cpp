#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/posts/posts_service.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <memory>
#include <regex>
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

drogon::HttpResponsePtr postsErrorResponse(const PostsError& e, const std::string& instance) {
    switch (e.code) {
        case PostsError::NotFound:
            return problemJson(drogon::k404NotFound, "not_found", e.detail, instance);
        case PostsError::Forbidden:
            return problemJson(drogon::k403Forbidden, "forbidden", e.detail, instance);
        case PostsError::BadRequest:
            return problemJson(drogon::k400BadRequest, "bad_request", e.detail, instance);
        case PostsError::InternalError:
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error", e.detail, instance);
    }
}

Json::Value postToJson(const Post& p) {
    Json::Value j(Json::objectValue);
    j["id"]              = static_cast<Json::Int64>(p.id);
    j["user_id"]         = static_cast<Json::Int64>(p.userId);
    j["body"]            = p.body;
    j["visibility"]      = toDbString(p.visibility);
    j["download_policy"] = toDbString(p.downloadPolicy);
    j["created_at"]      = p.createdAt;
    j["updated_at"]      = p.updatedAt;
    return j;
}

// Authorization 헤더 → user_id. 실패 시 nullopt + 401 응답을 호출자가 보냄
std::optional<std::int64_t> requireAuth(
    const std::shared_ptr<AuthService>& authService,
    const drogon::HttpRequestPtr& req,
    const std::function<void(const drogon::HttpResponsePtr&)>& callback,
    const std::string& instance) {
    auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
    if (!userId) {
        callback(problemJson(drogon::k401Unauthorized, "unauthorized",
                             "valid Bearer access token required", instance));
    }
    return userId;
}

// 마지막 path segment를 BIGINT로 파싱. /posts/123 → 123
std::optional<std::int64_t> parseIdFromPath(const std::string& path,
                                            const std::string& prefix) {
    if (path.size() <= prefix.size() ||
        path.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }
    auto rest = path.substr(prefix.size());
    // 다음 / 이후는 무시
    auto slash = rest.find('/');
    if (slash != std::string::npos) rest = rest.substr(0, slash);
    if (rest.empty()) return std::nullopt;
    try {
        return std::stoll(rest);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

void configurePostsRoutes(std::shared_ptr<AuthService> authService,
                          std::shared_ptr<PostsService> postsService) {
    // POST /posts — 글 작성
    drogon::app().registerHandler(
        "/posts",
        [authService, postsService](const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = requireAuth(authService, req, cb, "/posts");
            if (!userId) return;

            auto json = req->getJsonObject();
            if (!json) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "JSON body required", "/posts"));
                return;
            }
            CreatePostRequest creq;
            if (!(*json).isMember("body") || !(*json)["body"].isString()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "body (string) required", "/posts"));
                return;
            }
            creq.body = (*json)["body"].asString();

            std::string visStr = (*json).get("visibility", "private").asString();
            std::string dlpStr = (*json).get("download_policy", "owner_only").asString();
            auto vis = parseVisibility(visStr);
            auto dlp = parseDownloadPolicy(dlpStr);
            if (!vis) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "invalid visibility", "/posts"));
                return;
            }
            if (!dlp) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "invalid download_policy", "/posts"));
                return;
            }
            creq.visibility     = *vis;
            creq.downloadPolicy = *dlp;

            auto result = postsService->create(*userId, creq);
            if (auto* err = std::get_if<PostsError>(&result)) {
                cb(postsErrorResponse(*err, "/posts"));
            } else {
                auto resp = drogon::HttpResponse::newHttpJsonResponse(postToJson(std::get<Post>(result)));
                resp->setStatusCode(drogon::k201Created);
                cb(resp);
            }
        },
        {drogon::Post});

    // /posts/{id} — GET / PATCH / DELETE 단일 라우트
    drogon::app().registerHandlerViaRegex(
        "^/posts/([0-9]+)$",
        [authService, postsService](const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto pathStr = req->getPath();
            auto postId  = parseIdFromPath(pathStr, "/posts/");
            if (!postId) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "invalid post id", pathStr));
                return;
            }

            auto method = req->getMethod();
            if (method == drogon::Get) {
                // 비로그인도 public은 보임. viewerId = -1
                std::int64_t viewerId = -1;
                if (auto u = authService->verifyAccess(std::string(req->getHeader("Authorization")))) {
                    viewerId = *u;
                }
                auto result = postsService->get(viewerId, *postId);
                if (auto* err = std::get_if<PostsError>(&result)) {
                    cb(postsErrorResponse(*err, pathStr));
                } else {
                    cb(drogon::HttpResponse::newHttpJsonResponse(postToJson(std::get<Post>(result))));
                }
                return;
            }

            if (method == drogon::Patch) {
                auto userId = requireAuth(authService, req, cb, pathStr);
                if (!userId) return;
                auto json = req->getJsonObject();
                if (!json) {
                    cb(problemJson(drogon::k400BadRequest, "bad_request",
                                   "JSON body required", pathStr));
                    return;
                }
                UpdatePostRequest ureq;
                if ((*json).isMember("body") && (*json)["body"].isString()) {
                    ureq.body = (*json)["body"].asString();
                }
                if ((*json).isMember("visibility") && (*json)["visibility"].isString()) {
                    auto v = parseVisibility((*json)["visibility"].asString());
                    if (!v) {
                        cb(problemJson(drogon::k400BadRequest, "bad_request",
                                       "invalid visibility", pathStr));
                        return;
                    }
                    ureq.visibility = *v;
                }
                if ((*json).isMember("download_policy") && (*json)["download_policy"].isString()) {
                    auto d = parseDownloadPolicy((*json)["download_policy"].asString());
                    if (!d) {
                        cb(problemJson(drogon::k400BadRequest, "bad_request",
                                       "invalid download_policy", pathStr));
                        return;
                    }
                    ureq.downloadPolicy = *d;
                }
                auto result = postsService->update(*userId, *postId, ureq);
                if (auto* err = std::get_if<PostsError>(&result)) {
                    cb(postsErrorResponse(*err, pathStr));
                } else {
                    cb(drogon::HttpResponse::newHttpJsonResponse(postToJson(std::get<Post>(result))));
                }
                return;
            }

            if (method == drogon::Delete) {
                auto userId = requireAuth(authService, req, cb, pathStr);
                if (!userId) return;
                auto result = postsService->remove(*userId, *postId);
                if (auto* err = std::get_if<PostsError>(&result)) {
                    cb(postsErrorResponse(*err, pathStr));
                } else {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    cb(resp);
                }
                return;
            }

            cb(problemJson(drogon::k405MethodNotAllowed, "method_not_allowed",
                           "use GET/PATCH/DELETE", pathStr));
        },
        {drogon::Get, drogon::Patch, drogon::Delete});

    // GET /me/timeline — 본인 글 시간 역순
    drogon::app().registerHandler(
        "/me/timeline",
        [authService, postsService](const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = requireAuth(authService, req, cb, "/me/timeline");
            if (!userId) return;

            std::optional<std::int64_t> cursor;
            if (auto c = req->getParameter("cursor"); !c.empty()) {
                try { cursor = std::stoll(c); } catch (...) {}
            }
            int limit = 20;
            if (auto l = req->getParameter("limit"); !l.empty()) {
                try { limit = std::stoi(l); } catch (...) {}
            }

            auto result = postsService->timeline(*userId, *userId, cursor, limit);
            if (auto* err = std::get_if<PostsError>(&result)) {
                cb(postsErrorResponse(*err, "/me/timeline"));
            } else {
                const auto& page = std::get<TimelinePage>(result);
                Json::Value body(Json::objectValue);
                Json::Value items(Json::arrayValue);
                for (const auto& p : page.items) items.append(postToJson(p));
                body["items"] = items;
                if (page.nextCursor) {
                    body["next_cursor"] = static_cast<Json::Int64>(*page.nextCursor);
                } else {
                    body["next_cursor"] = Json::nullValue;
                }
                cb(drogon::HttpResponse::newHttpJsonResponse(body));
            }
        },
        {drogon::Get});

    // GET /me/search?q=... — 본인 글 키워드 검색 (FULLTEXT, MVP)
    drogon::app().registerHandler(
        "/me/search",
        [authService, postsService](const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = requireAuth(authService, req, cb, "/me/search");
            if (!userId) return;

            auto q = req->getParameter("q");
            int limit = 20;
            if (auto l = req->getParameter("limit"); !l.empty()) {
                try { limit = std::stoi(l); } catch (...) {}
            }
            auto result = postsService->searchOwn(*userId, q, limit);
            if (auto* err = std::get_if<PostsError>(&result)) {
                cb(postsErrorResponse(*err, "/me/search"));
                return;
            }
            const auto& items = std::get<std::vector<Post>>(result);
            Json::Value body(Json::objectValue);
            body["q"]     = q;
            body["count"] = static_cast<Json::Int64>(items.size());
            Json::Value arr(Json::arrayValue);
            for (const auto& p : items) arr.append(postToJson(p));
            body["items"] = arr;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});

    // GET /users/{id}/timeline — 타인 타임라인 (visibility 필터)
    drogon::app().registerHandlerViaRegex(
        "^/users/([0-9]+)/timeline$",
        [authService, postsService](const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto pathStr = req->getPath();
            // /users/{id}/timeline — id 파싱
            std::int64_t ownerId = 0;
            try {
                auto rest = pathStr.substr(std::string("/users/").size());
                auto slash = rest.find('/');
                ownerId = std::stoll(rest.substr(0, slash));
            } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "invalid user id", pathStr));
                return;
            }

            std::int64_t viewerId = -1;
            if (auto u = authService->verifyAccess(std::string(req->getHeader("Authorization")))) {
                viewerId = *u;
            }

            std::optional<std::int64_t> cursor;
            if (auto c = req->getParameter("cursor"); !c.empty()) {
                try { cursor = std::stoll(c); } catch (...) {}
            }
            int limit = 20;
            if (auto l = req->getParameter("limit"); !l.empty()) {
                try { limit = std::stoi(l); } catch (...) {}
            }

            auto result = postsService->timeline(viewerId, ownerId, cursor, limit);
            if (auto* err = std::get_if<PostsError>(&result)) {
                cb(postsErrorResponse(*err, pathStr));
            } else {
                const auto& page = std::get<TimelinePage>(result);
                Json::Value body(Json::objectValue);
                Json::Value items(Json::arrayValue);
                for (const auto& p : page.items) items.append(postToJson(p));
                body["items"] = items;
                if (page.nextCursor) {
                    body["next_cursor"] = static_cast<Json::Int64>(*page.nextCursor);
                } else {
                    body["next_cursor"] = Json::nullValue;
                }
                cb(drogon::HttpResponse::newHttpJsonResponse(body));
            }
        },
        {drogon::Get});
}

} // namespace monggle
