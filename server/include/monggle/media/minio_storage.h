#pragma once

#include "monggle/media/storage.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace monggle {

// S3 호환 (MinIO/AWS S3) 백엔드. minio-cpp 0.3.x 사용.
// 처리 중간단계는 로컬 임시 디렉토리(workDir)를 거친다 — OpenCV/ffmpeg가
// 파일 경로 기반이라 byte stream을 직접 다루기보다 임시 파일을 활용.
class MinioStorage : public IMediaStorage {
public:
    struct Options {
        std::string endpoint;     // e.g. "127.0.0.1:9000"
        std::string accessKey;
        std::string secretKey;
        std::string bucket;
        std::string region = "us-east-1";
        bool        secure = false; // MinIO 로컬은 HTTP
        std::string workDir = "/tmp/monggle-media";  // 임시 처리/다운로드용
        std::chrono::seconds presignTtl{300};        // 5분
    };

    explicit MinioStorage(const Options& opts);
    ~MinioStorage() override;

    bool putFile(const std::string& key, const std::string& localPath,
                 const std::string& mimeType) override;
    bool putBytes(const std::string& key, const char* data, std::size_t size,
                  const std::string& mimeType) override;
    std::optional<std::string> fetchToTemp(const std::string& key) override;
    bool deleteObject(const std::string& key) override;
    std::shared_ptr<drogon::HttpResponse>
    serve(const std::string& key, const std::string& mimeType,
          const std::string& disposition, const std::string& filename) override;
    std::string backendName() const override { return "minio"; }

    bool healthy() const { return healthy_.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool>     healthy_{false};
    Options               opts_;
};

} // namespace monggle
