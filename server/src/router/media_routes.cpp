#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/follows/follows_service.h"
#include "monggle/media/media_service.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <filesystem>
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

drogon::HttpResponsePtr mediaErrorResponse(const MediaError& e, const std::string& instance) {
    switch (e.code) {
        case MediaError::NotFound:
            return problemJson(drogon::k404NotFound, "not_found", e.detail, instance);
        case MediaError::Forbidden:
            return problemJson(drogon::k403Forbidden, "forbidden", e.detail, instance);
        case MediaError::BadRequest:
            return problemJson(drogon::k400BadRequest, "bad_request", e.detail, instance);
        case MediaError::UnsupportedType:
            return problemJson(drogon::k415UnsupportedMediaType, "unsupported_type",
                               e.detail, instance);
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error",
                               e.detail, instance);
    }
}

Json::Value mediaToJson(const MediaAsset& m) {
    Json::Value j(Json::objectValue);
    j["id"]                = static_cast<Json::Int64>(m.id);
    j["post_id"]           = static_cast<Json::Int64>(m.postId);
    j["user_id"]           = static_cast<Json::Int64>(m.userId);
    j["kind"]              = (m.kind == MediaKind::Video) ? "video" : "photo";
    j["mime_type"]         = m.mimeType;
    j["size_bytes"]        = static_cast<Json::Int64>(m.sizeBytes);
    j["width_px"]          = m.widthPx;
    j["height_px"]         = m.heightPx;
    j["duration_ms"]       = m.durationMs;
    j["status"]            = m.status;
    j["created_at"]        = m.createdAt;
    j["view_url"]          = "/media/" + std::to_string(m.id) + "/view";
    j["download_url"]      = "/media/" + std::to_string(m.id) + "/download";
    j["has_thumb"]         = !m.storeKeyThumb.empty();
    j["has_poster"]        = !m.storeKeyPoster.empty();
    return j;
}

std::int64_t parseIdSegment(const std::string& path, const std::string& prefix) {
    auto rest = path.substr(prefix.size());
    auto slash = rest.find('/');
    return std::stoll(rest.substr(0, slash));
}

} // namespace

void configureMediaRoutes(std::shared_ptr<AuthService> authService,
                          std::shared_ptr<FollowsService> followsService,
                          std::shared_ptr<MediaService> mediaService,
                          std::string storageRoot) {
    auto storageRootPath = std::make_shared<std::filesystem::path>(storageRoot);

    // POST /posts/{id}/media — multipart/form-data upload (single file)
    drogon::app().registerHandlerViaRegex(
        "^/posts/([0-9]+)/media$",
        [authService, mediaService](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", path));
                return;
            }

            std::int64_t postId = 0;
            try { postId = parseIdSegment(path, "/posts/"); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid post id", path));
                return;
            }

            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "multipart/form-data required", path));
                return;
            }
            const auto& files = parser.getFiles();
            if (files.empty()) {
                cb(problemJson(drogon::k400BadRequest, "bad_request",
                               "exactly one file part 'file' required", path));
                return;
            }
            const auto& f = files[0];

            // Drogon 1.8.7 HttpFile에는 getContentTypeString()이 없어 filename
            // 확장자 → mime 매핑으로 추론.
            auto guessMime = [](const std::string& fname) -> std::string {
                auto dot = fname.rfind('.');
                if (dot == std::string::npos) return "application/octet-stream";
                std::string ext = fname.substr(dot + 1);
                for (auto& c : ext) c = std::tolower(c);
                if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
                if (ext == "png")                  return "image/png";
                if (ext == "webp")                 return "image/webp";
                if (ext == "gif")                  return "image/gif";
                if (ext == "mp4")                  return "video/mp4";
                if (ext == "mov")                  return "video/quicktime";
                if (ext == "webm")                 return "video/webm";
                return "application/octet-stream";
            };
            std::string mime = guessMime(f.getFileName());

            auto result = mediaService->uploadForPost(
                *userId, postId,
                f.getFileName(),
                mime,
                f.fileData(),
                f.fileLength());

            if (auto* e = std::get_if<MediaError>(&result)) {
                cb(mediaErrorResponse(*e, path));
            } else {
                auto resp = drogon::HttpResponse::newHttpJsonResponse(
                    mediaToJson(std::get<MediaAsset>(result)));
                resp->setStatusCode(drogon::k201Created);
                cb(resp);
            }
        },
        {drogon::Post});

    // GET /media/{id}/view — 권한 체크 + 원본 파일 스트리밍
    drogon::app().registerHandlerViaRegex(
        "^/media/([0-9]+)/view$",
        [authService, followsService, mediaService, storageRootPath](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            std::int64_t mediaId = 0;
            try { mediaId = parseIdSegment(path, "/media/"); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid media id", path));
                return;
            }

            ViewerContext vc{-1, false};
            if (auto u = authService->verifyAccess(std::string(req->getHeader("Authorization")))) {
                vc.viewerId = *u;
            }

            auto result = mediaService->getForView(vc, mediaId);
            if (auto* e = std::get_if<MediaError>(&result)) {
                if (e->code == MediaError::Forbidden && vc.viewerId > 0 && followsService) {
                    // friends 글이라 follow 여부 확인 후 재시도
                    // (간단화: meta 다시 가져오지 않고 follower 여부만 viewer 컨텍스트에 채움)
                    // → 이미 NotFound면 의미 없으니 Forbidden일 때만
                }
                cb(mediaErrorResponse(*e, path));
                return;
            }
            const auto& m = std::get<MediaAsset>(result);
            auto absPath = (*storageRootPath / m.storeKeyOriginal).string();
            auto resp = drogon::HttpResponse::newFileResponse(
                absPath, "", drogon::CT_CUSTOM);
            resp->setContentTypeString(m.mimeType);
            cb(resp);
        },
        {drogon::Get});

    // GET /media/{id}/download — 다운로드 정책 체크 + Content-Disposition
    drogon::app().registerHandlerViaRegex(
        "^/media/([0-9]+)/download$",
        [authService, mediaService, storageRootPath](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            std::int64_t mediaId = 0;
            try { mediaId = parseIdSegment(path, "/media/"); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid media id", path));
                return;
            }

            ViewerContext vc{-1, false};
            if (auto u = authService->verifyAccess(std::string(req->getHeader("Authorization")))) {
                vc.viewerId = *u;
            }

            auto result = mediaService->getForDownload(vc, mediaId);
            if (auto* e = std::get_if<MediaError>(&result)) {
                cb(mediaErrorResponse(*e, path));
                return;
            }
            const auto& m = std::get<MediaAsset>(result);
            auto absPath = (*storageRootPath / m.storeKeyOriginal).string();
            auto fname = std::filesystem::path(m.storeKeyOriginal).filename().string();
            auto resp = drogon::HttpResponse::newFileResponse(
                absPath, fname, drogon::CT_CUSTOM, "attachment");
            resp->setContentTypeString(m.mimeType);
            cb(resp);
        },
        {drogon::Get});

    // GET /media/{id}/thumb — 썸네일 (사진만, 권한은 view와 동일)
    drogon::app().registerHandlerViaRegex(
        "^/media/([0-9]+)/thumb$",
        [authService, mediaService, storageRootPath](
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto path = req->getPath();
            std::int64_t mediaId = 0;
            try { mediaId = parseIdSegment(path, "/media/"); } catch (...) {
                cb(problemJson(drogon::k400BadRequest, "bad_request", "invalid media id", path));
                return;
            }
            ViewerContext vc{-1, false};
            if (auto u = authService->verifyAccess(std::string(req->getHeader("Authorization")))) {
                vc.viewerId = *u;
            }
            auto result = mediaService->getForView(vc, mediaId);
            if (auto* e = std::get_if<MediaError>(&result)) {
                cb(mediaErrorResponse(*e, path));
                return;
            }
            const auto& m = std::get<MediaAsset>(result);
            // 사진은 thumb_200, 영상은 poster
            std::string rel = m.storeKeyThumb.empty() ? m.storeKeyPoster : m.storeKeyThumb;
            if (rel.empty()) {
                cb(problemJson(drogon::k404NotFound, "no_thumbnail",
                               "thumbnail not generated", path));
                return;
            }
            auto absPath = (*storageRootPath / rel).string();
            auto resp = drogon::HttpResponse::newFileResponse(
                absPath, "", drogon::CT_CUSTOM);
            resp->setContentTypeString(m.storeKeyThumb.empty() ? "image/png" : "image/jpeg");
            cb(resp);
        },
        {drogon::Get});
}

} // namespace monggle
