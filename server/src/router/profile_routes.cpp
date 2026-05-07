#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/profile/profile_service.h"

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

drogon::HttpResponsePtr profileErrorResponse(const ProfileError& e, const std::string& instance) {
    switch (e.code) {
        case ProfileError::NotFound:
            return problemJson(drogon::k404NotFound, "not_found", e.detail, instance);
        case ProfileError::BadRequest:
            return problemJson(drogon::k400BadRequest, "bad_request", e.detail, instance);
        case ProfileError::UnsupportedType:
            return problemJson(drogon::k415UnsupportedMediaType, "unsupported_type",
                               e.detail, instance);
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error",
                               e.detail, instance);
    }
}

std::int64_t parseIdSegment(const std::string& path, const std::string& prefix) {
    auto rest = path.substr(prefix.size());
    auto slash = rest.find('/');
    return std::stoll(rest.substr(0, slash));
}

} // namespace

void configureProfileRoutes(std::shared_ptr<AuthService> authService,
                            std::shared_ptr<ProfileService> profileService) {
    // PUT /me/avatar — multipart 파일 1개 업로드
    drogon::app().registerHandler(
        "/me/avatar",
        [authService, profileService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/avatar"));
                return;
            }

            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "multipart/form-data required", "/me/avatar"));
                return;
            }
            const auto& files = parser.getFiles();
            if (files.empty()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "exactly one file part required", "/me/avatar"));
                return;
            }
            const auto& f = files[0];

            // 확장자에서 mime 추정. 모르면 image/* 로 가정해 OpenCV가 직접 판단하도록 위임.
            auto guessMime = [](const std::string& fname) -> std::string {
                auto dot = fname.rfind('.');
                if (dot == std::string::npos) return "image/jpeg";
                std::string ext = fname.substr(dot + 1);
                for (auto& c : ext) c = std::tolower(c);
                if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
                if (ext == "png")                  return "image/png";
                if (ext == "webp")                 return "image/webp";
                if (ext == "gif")                  return "image/gif";
                if (ext == "bmp")                  return "image/bmp";
                if (ext == "heic" || ext == "heif") return "image/heic";
                return "image/jpeg";   // OpenCV imdecode가 magic bytes로 자동 판단
            };
            std::string mime = guessMime(f.getFileName());

            auto r = profileService->updateAvatar(*userId, mime, f.fileData(), f.fileLength());
            if (auto* e = std::get_if<ProfileError>(&r)) {
                cb(profileErrorResponse(*e, "/me/avatar"));
                return;
            }
            Json::Value body;
            body["avatar_path"] = std::get<std::string>(r);
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Put, drogon::Post});

    // PATCH /me — display_name 변경 (프로필 수정 페이지)
    drogon::app().registerHandler(
        "/me",
        [authService, profileService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me"));
                return;
            }
            auto json = req->getJsonObject();
            if (!json || !json->isMember("display_name") || !(*json)["display_name"].isString()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "display_name (string) required", "/me"));
                return;
            }
            std::string name = (*json)["display_name"].asString();
            auto r = profileService->updateDisplayName(*userId, name);
            if (auto* e = std::get_if<ProfileError>(&r)) {
                cb(profileErrorResponse(*e, "/me"));
                return;
            }
            Json::Value body;
            body["display_name"] = name;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Patch});

    // PATCH /me/password — 비밀번호 변경 (기존 비번 검증)
    drogon::app().registerHandler(
        "/me/password",
        [authService, profileService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/password"));
                return;
            }
            auto json = req->getJsonObject();
            if (!json
                || !json->isMember("old_password") || !(*json)["old_password"].isString()
                || !json->isMember("new_password") || !(*json)["new_password"].isString()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "old_password and new_password (string) required", "/me/password"));
                return;
            }
            auto r = profileService->changePassword(
                *userId,
                (*json)["old_password"].asString(),
                (*json)["new_password"].asString());
            if (auto* e = std::get_if<ProfileError>(&r)) {
                cb(profileErrorResponse(*e, "/me/password"));
            } else {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                cb(resp);
            }
        },
        {drogon::Patch});

    // POST /me/verify-password — 프로필 수정 잠금 해제용 (변경 X, 검증만)
    drogon::app().registerHandler(
        "/me/verify-password",
        [authService, profileService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/verify-password"));
                return;
            }
            auto json = req->getJsonObject();
            if (!json || !json->isMember("password") || !(*json)["password"].isString()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "password (string) required", "/me/verify-password"));
                return;
            }
            bool ok = profileService->verifyPassword(*userId, (*json)["password"].asString());
            Json::Value body;
            body["ok"] = ok;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(ok ? drogon::k200OK : drogon::k401Unauthorized);
            cb(resp);
        },
        {drogon::Post});

    // GET /users/{id}/avatar — 공개. 없으면 404 → 프론트는 첫 글자 fallback.
    drogon::app().registerHandlerViaRegex(
        "^/users/([0-9]+)/avatar$",
        [profileService](const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            std::int64_t userId = 0;
            try { userId = parseIdSegment(req->getPath(), "/users/"); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "invalid user id", req->getPath()));
                return;
            }
            auto file = profileService->avatarFile(userId);
            if (!file) {
                cb(problemJson(drogon::k404NotFound, "no_avatar",
                               "avatar not set", req->getPath()));
                return;
            }
            auto resp = drogon::HttpResponse::newFileResponse(*file, "", drogon::CT_CUSTOM);
            resp->setContentTypeString("image/jpeg");
            cb(resp);
        },
        {drogon::Get});
}

} // namespace monggle
