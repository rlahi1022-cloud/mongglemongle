#include "monggle/profile/profile_service.h"
#include "monggle/auth/password_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <trantor/utils/Logger.h>

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace monggle {

namespace {

drogon::orm::DbClientPtr db() { return drogon::app().getDbClient("monggle_db"); }

bool isImageMime(const std::string& m) {
    return m.rfind("image/", 0) == 0;
}

void ensureDir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

} // namespace

ProfileService::ProfileService(std::string storageRoot) : storageRoot_(std::move(storageRoot)) {
    ensureDir(fs::path(storageRoot_) / "avatars");
}

std::optional<ProfileRow> ProfileService::get(std::int64_t userId) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT id, email, display_name, avatar_path "
            "FROM users WHERE id = ? LIMIT 1", userId);
        if (rows.size() == 0) return std::nullopt;
        ProfileRow p;
        p.id          = rows[0]["id"].as<std::int64_t>();
        p.email       = rows[0]["email"].as<std::string>();
        p.displayName = rows[0]["display_name"].as<std::string>();
        p.avatarPath  = rows[0]["avatar_path"].isNull()
                            ? std::string{}
                            : rows[0]["avatar_path"].as<std::string>();
        return p;
    } catch (...) {
        return std::nullopt;
    }
}

PResult<std::string> ProfileService::updateAvatar(std::int64_t userId,
                                                    const std::string& mime,
                                                    const char* bytes,
                                                    std::size_t size) {
    if (size == 0)               return ProfileError{ProfileError::BadRequest, "empty body"};
    if (size > 8 * 1024 * 1024)  return ProfileError{ProfileError::BadRequest, "max 8MB"};
    if (!isImageMime(mime))      return ProfileError{ProfileError::UnsupportedType, "image/* required"};

    try {
        // OpenCV로 메모리에서 디코드
        std::vector<unsigned char> buf(bytes, bytes + size);
        cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (img.empty()) {
            return ProfileError{ProfileError::BadRequest, "could not decode image"};
        }

        // 정사각형 중앙 크롭
        int side = std::min(img.cols, img.rows);
        int x = (img.cols - side) / 2;
        int y = (img.rows - side) / 2;
        cv::Mat square(img, cv::Rect(x, y, side, side));

        // 256x256 리사이즈
        cv::Mat resized;
        cv::resize(square, resized, cv::Size(256, 256), 0, 0, cv::INTER_AREA);

        // 저장 (항상 jpg, 확장자 고정)
        std::string rel = "avatars/" + std::to_string(userId) + ".jpg";
        fs::path abs = fs::path(storageRoot_) / rel;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 88};
        if (!cv::imwrite(abs.string(), resized, params)) {
            return ProfileError{ProfileError::InternalError, "imwrite failed"};
        }

        // DB 갱신
        db()->execSqlSync(
            "UPDATE users SET avatar_path = ? WHERE id = ?",
            rel, userId);

        return rel;
    } catch (const drogon::orm::DrogonDbException& e) {
        return ProfileError{ProfileError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return ProfileError{ProfileError::InternalError, e.what()};
    }
}

PResult<bool> ProfileService::changePassword(std::int64_t userId,
                                              const std::string& oldPassword,
                                              const std::string& newPassword) {
    if (newPassword.size() < 4)  return ProfileError{ProfileError::BadRequest, "new password too short"};
    if (newPassword.size() > 100) return ProfileError{ProfileError::BadRequest, "new password too long"};
    try {
        auto rows = db()->execSqlSync(
            "SELECT password_hash FROM users WHERE id = ? LIMIT 1", userId);
        if (rows.size() == 0) return ProfileError{ProfileError::NotFound, "user not found"};
        auto hashed = rows[0]["password_hash"].as<std::string>();
        if (!PasswordService::verify(oldPassword, hashed)) {
            return ProfileError{ProfileError::BadRequest, "current password mismatch"};
        }
        auto newHash = PasswordService::hash(newPassword);
        db()->execSqlSync(
            "UPDATE users SET password_hash = ? WHERE id = ?", newHash, userId);
        // 모든 refresh token 무효화 (회전 강제)
        try {
            db()->execSqlSync(
                "UPDATE refresh_tokens SET revoked = 1 WHERE user_id = ? AND revoked = 0",
                userId);
        } catch (...) {}
        return true;
    } catch (const std::exception& e) {
        return ProfileError{ProfileError::InternalError, e.what()};
    }
}

PResult<bool> ProfileService::updateDisplayName(std::int64_t userId, const std::string& newName) {
    if (newName.empty())          return ProfileError{ProfileError::BadRequest, "name required"};
    if (newName.size() > 100)     return ProfileError{ProfileError::BadRequest, "name too long"};
    try {
        db()->execSqlSync(
            "UPDATE users SET display_name = ? WHERE id = ?", newName, userId);
        return true;
    } catch (const std::exception& e) {
        return ProfileError{ProfileError::InternalError, e.what()};
    }
}

bool ProfileService::verifyPassword(std::int64_t userId, const std::string& password) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT password_hash FROM users WHERE id = ? LIMIT 1", userId);
        if (rows.size() == 0) return false;
        return PasswordService::verify(password, rows[0]["password_hash"].as<std::string>());
    } catch (...) { return false; }
}

std::optional<std::string> ProfileService::avatarFile(std::int64_t userId) {
    auto p = get(userId);
    if (!p || p->avatarPath.empty()) return std::nullopt;
    fs::path abs = fs::path(storageRoot_) / p->avatarPath;
    if (!fs::exists(abs)) return std::nullopt;
    return abs.string();
}

} // namespace monggle
