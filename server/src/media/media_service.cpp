#include "monggle/media/media_service.h"
#include "monggle/blocks/blocks_service.h"
#include "monggle/follows/follows_service.h"
#include "monggle/media/permissions.h"
#include "monggle/media/storage.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <trantor/utils/Logger.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace monggle {

namespace {

drogon::orm::DbClientPtr db() {
    return drogon::app().getDbClient("monggle_db");
}

bool startsWith(const std::string& s, const char* p) {
    auto n = std::strlen(p);
    return s.size() >= n && std::equal(p, p + n, s.begin());
}

std::optional<MediaKind> kindFromMime(const std::string& mime) {
    if (startsWith(mime, "image/")) return MediaKind::Photo;
    if (startsWith(mime, "video/")) return MediaKind::Video;
    return std::nullopt;
}

const char* kindToDb(MediaKind k) {
    return k == MediaKind::Photo ? "photo" : "video";
}

std::string sanitizeExt(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return "";
    std::string ext = filename.substr(dot + 1);
    std::string ok;
    for (char c : ext) {
        if (std::isalnum(static_cast<unsigned char>(c))) ok.push_back(std::tolower(c));
        if (ok.size() >= 8) break;
    }
    return ok;
}

void ensureDir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

void writeFile(const fs::path& p, const char* data, std::size_t n) {
    std::ofstream out(p, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open for write: " + p.string());
    out.write(data, static_cast<std::streamsize>(n));
    if (!out) throw std::runtime_error("write failed: " + p.string());
}

// 사진: 200/800 너비로 비율 유지 리사이즈 (jpg). 실패하면 빈 경로 반환.
struct ThumbResult {
    std::string thumbRelPath;       // 200px
    int width = 0, height = 0;
};

ThumbResult generatePhotoThumb(const fs::path& original,
                               const fs::path& outDir) {
    ThumbResult r;
    cv::Mat img = cv::imread(original.string(), cv::IMREAD_COLOR);
    if (img.empty()) {
        LOG_WARN << "[media] OpenCV cannot decode " << original;
        return r;
    }
    r.width = img.cols;
    r.height = img.rows;

    auto resizeAndWrite = [&](int targetWidth, const std::string& name) {
        int w = std::min(targetWidth, img.cols);
        int h = static_cast<int>(static_cast<double>(img.rows) * w / img.cols);
        cv::Mat dst;
        cv::resize(img, dst, cv::Size(w, h), 0, 0, cv::INTER_AREA);
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 85};
        auto path = outDir / name;
        cv::imwrite(path.string(), dst, params);
        return path;
    };

    auto thumb200 = resizeAndWrite(200,  "thumb_200.jpg");
    resizeAndWrite(800, "thumb_800.jpg");

    // outDir 기준에서 storage_root 까지의 상대경로는 호출자가 계산.
    r.thumbRelPath = thumb200.string();
    return r;
}

// 영상 첫 프레임 추출 — ffmpeg 시스템 호출.
// 반환: 절대경로 string 또는 빈 string.
std::string generateVideoPoster(const fs::path& original, const fs::path& outDir) {
    auto poster = outDir / "poster.png";
    // -y overwrite, -ss 0 첫 프레임, -frames:v 1
    std::string cmd = "ffmpeg -y -loglevel error -ss 0 -i \"" + original.string() +
                      "\" -frames:v 1 \"" + poster.string() + "\"";
    int rc = std::system(cmd.c_str());
    if (rc != 0 || !fs::exists(poster)) {
        LOG_WARN << "[media] ffmpeg poster failed rc=" << rc;
        return {};
    }
    return poster.string();
}

std::string generateAssetSubdir() {
    // 12자리 hex (충돌 거의 없음). 파일경로의 traversal은 발생 불가 (영숫자만).
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dist;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%012llx",
                  static_cast<unsigned long long>(dist(gen) & 0xFFFFFFFFFFFFull));
    return std::string(buf, 12);
}

// 권한 체크 — 포스트 visibility + (다운로드 시) download_policy
struct PostMeta {
    std::int64_t userId;
    std::string visibility;
    std::string downloadPolicy;
    bool deleted;
};

std::optional<PostMeta> loadPostMeta(std::int64_t postId) {
    auto rows = db()->execSqlSync(
        "SELECT user_id, visibility, download_policy, deleted_at "
        "FROM posts WHERE id = ?", postId);
    if (rows.size() == 0) return std::nullopt;
    PostMeta m;
    m.userId         = rows[0]["user_id"].as<std::int64_t>();
    m.visibility     = rows[0]["visibility"].as<std::string>();
    m.downloadPolicy = rows[0]["download_policy"].as<std::string>();
    m.deleted        = !rows[0]["deleted_at"].isNull();
    return m;
}

bool canViewPost(const ViewerContext& vc, const PostMeta& p,
                 FollowsService* follows, BlocksService* blocks) {
    bool isFollower = vc.isFollowerOfAuthor ||
                      (follows && vc.viewerId > 0 && follows->isFollower(vc.viewerId, p.userId));
    bool isBlocked  = blocks && vc.viewerId > 0 && blocks->isBlocked(vc.viewerId, p.userId);
    return permissions::canView(vc.viewerId, p.userId, p.visibility, isFollower, isBlocked);
}

bool canDownload(const ViewerContext& vc, const PostMeta& p,
                 FollowsService* follows, BlocksService* blocks) {
    bool isFollower = vc.isFollowerOfAuthor ||
                      (follows && vc.viewerId > 0 && follows->isFollower(vc.viewerId, p.userId));
    bool isBlocked  = blocks && vc.viewerId > 0 && blocks->isBlocked(vc.viewerId, p.userId);
    return permissions::canDownload(vc.viewerId, p.userId, p.visibility, p.downloadPolicy,
                                    isFollower, isBlocked);
}

MediaAsset rowToMedia(const drogon::orm::Row& r) {
    MediaAsset m;
    m.id             = r["id"].as<std::int64_t>();
    m.postId         = r["post_id"].as<std::int64_t>();
    m.userId         = r["user_id"].as<std::int64_t>();
    m.kind           = (r["kind"].as<std::string>() == "video") ? MediaKind::Video : MediaKind::Photo;
    m.storeKeyOriginal = r["s3_key_original"].isNull() ? "" : r["s3_key_original"].as<std::string>();
    m.storeKeyThumb    = r["s3_key_thumb"].isNull()    ? "" : r["s3_key_thumb"].as<std::string>();
    m.storeKeyPoster   = r["s3_key_poster"].isNull()   ? "" : r["s3_key_poster"].as<std::string>();
    m.mimeType         = r["mime_type"].isNull() ? "" : r["mime_type"].as<std::string>();
    m.sizeBytes        = r["size_bytes"].isNull() ? 0 : r["size_bytes"].as<std::int64_t>();
    m.widthPx          = r["width_px"].isNull()   ? 0 : r["width_px"].as<int>();
    m.heightPx         = r["height_px"].isNull()  ? 0 : r["height_px"].as<int>();
    m.durationMs       = r["duration_ms"].isNull()? 0 : r["duration_ms"].as<int>();
    m.status           = r["status"].as<std::string>();
    m.createdAt        = r["created_at"].as<std::string>();
    return m;
}

} // namespace

MediaService::MediaService(std::string storageRoot,
                           std::shared_ptr<IMediaStorage> storage,
                           std::shared_ptr<FollowsService> follows,
                           std::shared_ptr<BlocksService> blocks)
    : storageRoot_(std::move(storageRoot)),
      storage_(std::move(storage)),
      follows_(std::move(follows)),
      blocks_(std::move(blocks)) {
    ensureDir(storageRoot_);
}

MResult<MediaAsset> MediaService::uploadForPost(std::int64_t authorId,
                                                std::int64_t postId,
                                                const std::string& filename,
                                                const std::string& mimeType,
                                                const char*       bytes,
                                                std::size_t       size) {
    if (size == 0) return MediaError{MediaError::BadRequest, "empty body"};

    auto kind = kindFromMime(mimeType);
    if (!kind) return MediaError{MediaError::UnsupportedType, "image/* or video/* required"};

    try {
        // 1) post 권한 체크: 본인 글에만 첨부 허용
        auto post = loadPostMeta(postId);
        if (!post)              return MediaError{MediaError::NotFound,  "post not found"};
        if (post->deleted)      return MediaError{MediaError::NotFound,  "post deleted"};
        if (post->userId != authorId)
            return MediaError{MediaError::Forbidden, "not the author"};

        // 2) media_assets row 먼저 (id 확보) — pending 상태
        auto inserted = db()->execSqlSync(
            "INSERT INTO media_assets (post_id, user_id, kind, mime_type, size_bytes, status) "
            "VALUES (?, ?, ?, ?, ?, 'pending')",
            postId, authorId, std::string(kindToDb(*kind)),
            mimeType, static_cast<std::int64_t>(size));
        std::int64_t mediaId = inserted.insertId();

        // 3) 디스크 저장 — media/users/{u}/posts/{p}/{asset_subdir}/original.{ext}
        auto sub = generateAssetSubdir();
        fs::path dir = fs::path(storageRoot_) /
                       ("users/" + std::to_string(authorId)) /
                       ("posts/" + std::to_string(postId)) /
                       sub;
        ensureDir(dir);

        std::string ext = sanitizeExt(filename);
        if (ext.empty()) ext = (*kind == MediaKind::Photo) ? "bin" : "mp4";
        fs::path original = dir / ("original." + ext);
        writeFile(original, bytes, size);

        // 4) 썸네일/포스터
        std::string thumbAbs;
        std::string posterAbs;
        int width = 0, height = 0;
        try {
            if (*kind == MediaKind::Photo) {
                auto t = generatePhotoThumb(original, dir);
                thumbAbs = t.thumbRelPath;
                width = t.width;
                height = t.height;
            } else {
                posterAbs = generateVideoPoster(original, dir);
            }
        } catch (const std::exception& e) {
            LOG_WARN << "[media] thumb gen exception: " << e.what();
        }

        // 5) DB 갱신 — store_key_* 는 상대경로(storage_root 기준)
        auto rel = [&](const std::string& abs) -> std::string {
            if (abs.empty()) return "";
            std::error_code ec;
            return fs::relative(abs, storageRoot_, ec).string();
        };
        std::string relOriginal = rel(original.string());
        std::string relThumb    = rel(thumbAbs);
        std::string relPoster   = rel(posterAbs);

        // Storage 백엔드에 업로드. LocalFs면 root_와 storageRoot_가 같아 no-op,
        // Minio면 실제 PutObject 발생.
        if (storage_ && storage_->backendName() != "local-fs") {
            if (!relOriginal.empty()) storage_->putFile(relOriginal, original.string(), mimeType);
            if (!relThumb.empty())    storage_->putFile(relThumb,    thumbAbs,           "image/jpeg");
            if (!relPoster.empty())   storage_->putFile(relPoster,   posterAbs,          "image/png");
            // 800px 변형도 함께 업로드 (썸네일 외에 list 카드용)
            std::string thumb800 = (dir / "thumb_800.jpg").string();
            if (fs::exists(thumb800)) {
                storage_->putFile(rel(thumb800), thumb800, "image/jpeg");
            }
        }

        db()->execSqlSync(
            "UPDATE media_assets SET "
            "  s3_key_original = ?, "
            "  s3_key_thumb    = ?, "
            "  s3_key_poster   = ?, "
            "  width_px        = ?, "
            "  height_px       = ?, "
            "  status          = 'ready' "
            "WHERE id = ?",
            relOriginal,
            relThumb.empty()  ? std::string() : relThumb,
            relPoster.empty() ? std::string() : relPoster,
            width, height, mediaId);

        // 6) post_events에 media_added 누적 (시점 복원용)
        std::string payload = R"({"media_id":)" + std::to_string(mediaId) +
                              R"(,"kind":")" + kindToDb(*kind) +
                              R"("})";
        db()->execSqlSync(
            "INSERT INTO post_events (user_id, post_id, event_type, payload_json) "
            "VALUES (?, ?, 'media_added', ?)",
            authorId, postId, payload);

        // 7) 응답용 SELECT
        auto rows = db()->execSqlSync(
            "SELECT * FROM media_assets WHERE id = ?", mediaId);
        return rowToMedia(rows[0]);
    } catch (const drogon::orm::DrogonDbException& e) {
        return MediaError{MediaError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return MediaError{MediaError::InternalError, e.what()};
    }
}

MResult<MediaAsset> MediaService::getForView(const ViewerContext& vc, std::int64_t mediaId) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT * FROM media_assets WHERE id = ?", mediaId);
        if (rows.size() == 0) return MediaError{MediaError::NotFound, "media not found"};
        auto m = rowToMedia(rows[0]);

        auto post = loadPostMeta(m.postId);
        if (!post || post->deleted) return MediaError{MediaError::NotFound, "media not found"};
        if (!canViewPost(vc, *post, follows_.get(), blocks_.get()))
            return MediaError{MediaError::Forbidden, "no permission"};
        return m;
    } catch (const std::exception& e) {
        return MediaError{MediaError::InternalError, e.what()};
    }
}

MResult<std::vector<MediaAsset>> MediaService::listForPost(const ViewerContext& vc, std::int64_t postId) {
    try {
        auto post = loadPostMeta(postId);
        if (!post || post->deleted) return MediaError{MediaError::NotFound, "post not found"};
        if (!canViewPost(vc, *post, follows_.get(), blocks_.get()))
            return MediaError{MediaError::Forbidden, "no permission"};

        auto rows = db()->execSqlSync(
            "SELECT * FROM media_assets WHERE post_id = ? AND status = 'ready' ORDER BY id",
            postId);
        std::vector<MediaAsset> out;
        out.reserve(rows.size());
        for (const auto& r : rows) out.push_back(rowToMedia(r));
        return out;
    } catch (const std::exception& e) {
        return MediaError{MediaError::InternalError, e.what()};
    }
}

MResult<MediaAsset> MediaService::getForDownload(const ViewerContext& vc, std::int64_t mediaId) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT * FROM media_assets WHERE id = ?", mediaId);
        if (rows.size() == 0) return MediaError{MediaError::NotFound, "media not found"};
        auto m = rowToMedia(rows[0]);

        auto post = loadPostMeta(m.postId);
        if (!post || post->deleted) return MediaError{MediaError::NotFound, "media not found"};
        if (!canDownload(vc, *post, follows_.get(), blocks_.get()))
            return MediaError{MediaError::Forbidden, "download not allowed by author policy"};
        return m;
    } catch (const std::exception& e) {
        return MediaError{MediaError::InternalError, e.what()};
    }
}

} // namespace monggle
