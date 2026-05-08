#include "monggle/media/local_fs_storage.h"

#include <drogon/HttpResponse.h>

#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace monggle {

namespace {

fs::path joinKey(const std::string& root, const std::string& key) {
    return fs::path(root) / key;
}

void ensureParent(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
}

bool copyFileTo(const std::string& src, const fs::path& dst) {
    ensureParent(dst);
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

bool writeBytes(const fs::path& dst, const char* data, std::size_t n) {
    ensureParent(dst);
    std::ofstream out(dst, std::ios::binary);
    if (!out) return false;
    out.write(data, static_cast<std::streamsize>(n));
    return out.good();
}

} // namespace

LocalFsStorage::LocalFsStorage(std::string storageRoot)
    : storageRoot_(std::move(storageRoot)) {
    std::error_code ec;
    fs::create_directories(storageRoot_, ec);
}

bool LocalFsStorage::putFile(const std::string& key, const std::string& localPath,
                             const std::string& /*mimeType*/) {
    auto target = joinKey(storageRoot_, key);
    if (fs::path(localPath) == target) return true; // already in place
    return copyFileTo(localPath, target);
}

bool LocalFsStorage::putBytes(const std::string& key, const char* data, std::size_t size,
                              const std::string& /*mimeType*/) {
    return writeBytes(joinKey(storageRoot_, key), data, size);
}

std::optional<std::string> LocalFsStorage::fetchToTemp(const std::string& key) {
    auto p = joinKey(storageRoot_, key);
    if (!fs::exists(p)) return std::nullopt;
    return p.string(); // 로컬 백엔드는 그대로 반환 — 추가 복사 불필요
}

bool LocalFsStorage::deleteObject(const std::string& key) {
    std::error_code ec;
    fs::remove(joinKey(storageRoot_, key), ec);
    return true; // 멱등
}

std::shared_ptr<drogon::HttpResponse>
LocalFsStorage::serve(const std::string& key, const std::string& mimeType,
                      const std::string& disposition, const std::string& filename) {
    auto p = joinKey(storageRoot_, key);
    if (!fs::exists(p)) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k404NotFound);
        return resp;
    }

    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k500InternalServerError);
        return resp;
    }
    std::streamsize sz = in.tellg();
    in.seekg(0);
    std::string body(static_cast<std::size_t>(sz), '\0');
    in.read(body.data(), sz);

    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody(std::move(body));
    if (!mimeType.empty()) resp->setContentTypeString(mimeType);
    if (disposition == "attachment") {
        std::ostringstream cd;
        cd << "attachment";
        if (!filename.empty()) cd << "; filename=\"" << filename << "\"";
        resp->addHeader("Content-Disposition", cd.str());
    }
    return resp;
}

} // namespace monggle
