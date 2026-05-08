#pragma once

#include "monggle/media/storage.h"

#include <string>

namespace monggle {

// 단일 노드 로컬 파일시스템 백엔드.
// key는 storageRoot 아래 상대 경로로 매핑된다 (e.g. "users/7/posts/42/abc/original.jpg").
class LocalFsStorage : public IMediaStorage {
public:
    explicit LocalFsStorage(std::string storageRoot);

    bool putFile(const std::string& key, const std::string& localPath,
                 const std::string& mimeType) override;
    bool putBytes(const std::string& key, const char* data, std::size_t size,
                  const std::string& mimeType) override;
    std::optional<std::string> fetchToTemp(const std::string& key) override;
    bool deleteObject(const std::string& key) override;
    std::shared_ptr<drogon::HttpResponse>
    serve(const std::string& key, const std::string& mimeType,
          const std::string& disposition, const std::string& filename) override;
    std::string backendName() const override { return "local-fs"; }

    const std::string& root() const { return storageRoot_; }

private:
    std::string storageRoot_;
};

} // namespace monggle
