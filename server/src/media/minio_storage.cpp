#include "monggle/media/minio_storage.h"

#include <miniocpp/args.h>
#include <miniocpp/client.h>
#include <miniocpp/providers.h>
#include <miniocpp/request.h>
#include <miniocpp/types.h>

#include <drogon/HttpResponse.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace monggle {

struct MinioStorage::Impl {
    minio::s3::BaseUrl                       baseUrl;
    std::unique_ptr<minio::creds::StaticProvider> provider;
    std::unique_ptr<minio::s3::Client>       client;

    Impl(const std::string& endpoint, bool secure,
         const std::string& accessKey, const std::string& secretKey)
        : baseUrl(endpoint, secure) {
        provider = std::make_unique<minio::creds::StaticProvider>(accessKey, secretKey);
        client   = std::make_unique<minio::s3::Client>(baseUrl, provider.get());
    }
};

namespace {

std::string randomTempName() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dist;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(dist(gen)));
    return std::string(buf);
}

void ensureDir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

// 버킷이 없으면 생성. 응답 없거나 다른 서비스가 9000을 잡고 있으면 false 반환.
bool ensureBucket(minio::s3::Client& client, const std::string& bucket) {
    minio::s3::BucketExistsArgs ea;
    ea.bucket = bucket;
    auto exists = client.BucketExists(ea);
    if (!exists) {
        std::cerr << "[minio] BucketExists failed: " << exists.Error().String() << std::endl;
        return false;
    }
    if (exists.exist) return true;

    minio::s3::MakeBucketArgs ma;
    ma.bucket = bucket;
    auto made = client.MakeBucket(ma);
    if (!made) {
        std::cerr << "[minio] MakeBucket failed: " << made.Error().String() << std::endl;
        return false;
    }
    return true;
}

} // namespace

MinioStorage::MinioStorage(const Options& opts) : opts_(opts) {
    try {
        impl_ = std::make_unique<Impl>(opts.endpoint, opts.secure,
                                       opts.accessKey, opts.secretKey);
        ensureDir(opts_.workDir);
        // 버킷 존재/생성 실패 시 (다른 서비스가 같은 포트를 잡거나 자격 증명 오류 등)
        // healthy=false로 두어 main에서 LocalFsStorage 로 fallback 되게 함.
        healthy_.store(ensureBucket(*impl_->client, opts.bucket));
    } catch (const std::exception& e) {
        std::cerr << "[minio] init failed: " << e.what()
                  << " — degrading to no-op" << std::endl;
        healthy_.store(false);
    }
}

MinioStorage::~MinioStorage() = default;

bool MinioStorage::putFile(const std::string& key, const std::string& localPath,
                           const std::string& mimeType) {
    if (!healthy_.load() || !impl_) return false;
    try {
        std::ifstream in(localPath, std::ios::binary | std::ios::ate);
        if (!in) return false;
        std::streamsize sz = in.tellg();
        in.seekg(0);

        minio::s3::PutObjectArgs args(in, sz, 0);
        args.bucket       = opts_.bucket;
        args.object       = key;
        if (!mimeType.empty()) args.content_type = mimeType;

        auto resp = impl_->client->PutObject(args);
        if (!resp) {
            std::cerr << "[minio] PutObject(" << key << ") failed: "
                      << resp.Error().String() << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[minio] putFile error: " << e.what() << std::endl;
        healthy_.store(false);
        return false;
    }
}

bool MinioStorage::putBytes(const std::string& key, const char* data, std::size_t size,
                            const std::string& mimeType) {
    // PutObjectArgs는 istream 기반이라 stringstream 경유.
    if (!healthy_.load() || !impl_) return false;
    try {
        std::stringstream ss;
        ss.write(data, static_cast<std::streamsize>(size));
        ss.seekg(0);

        minio::s3::PutObjectArgs args(ss, static_cast<long>(size), 0);
        args.bucket = opts_.bucket;
        args.object = key;
        if (!mimeType.empty()) args.content_type = mimeType;

        auto resp = impl_->client->PutObject(args);
        if (!resp) {
            std::cerr << "[minio] PutObject(" << key << ") failed: "
                      << resp.Error().String() << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[minio] putBytes error: " << e.what() << std::endl;
        healthy_.store(false);
        return false;
    }
}

std::optional<std::string> MinioStorage::fetchToTemp(const std::string& key) {
    if (!healthy_.load() || !impl_) return std::nullopt;
    try {
        auto target = fs::path(opts_.workDir) / (randomTempName() + "_" +
            fs::path(key).filename().string());
        ensureDir(target.parent_path());

        minio::s3::DownloadObjectArgs args;
        args.bucket   = opts_.bucket;
        args.object   = key;
        args.filename = target.string();
        args.overwrite = true;

        auto resp = impl_->client->DownloadObject(args);
        if (!resp) {
            std::cerr << "[minio] DownloadObject(" << key << ") failed: "
                      << resp.Error().String() << std::endl;
            return std::nullopt;
        }
        return target.string();
    } catch (const std::exception& e) {
        std::cerr << "[minio] fetch error: " << e.what() << std::endl;
        healthy_.store(false);
        return std::nullopt;
    }
}

bool MinioStorage::deleteObject(const std::string& key) {
    if (!healthy_.load() || !impl_) return false;
    try {
        minio::s3::RemoveObjectArgs args;
        args.bucket = opts_.bucket;
        args.object = key;
        auto resp = impl_->client->RemoveObject(args);
        return static_cast<bool>(resp);
    } catch (const std::exception& e) {
        std::cerr << "[minio] delete error: " << e.what() << std::endl;
        healthy_.store(false);
        return false;
    }
}

std::shared_ptr<drogon::HttpResponse>
MinioStorage::serve(const std::string& key, const std::string& /*mimeType*/,
                    const std::string& disposition, const std::string& filename) {
    if (!healthy_.load() || !impl_) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k503ServiceUnavailable);
        return resp;
    }
    try {
        minio::s3::GetPresignedObjectUrlArgs args;
        args.bucket           = opts_.bucket;
        args.object           = key;
        args.method           = minio::http::Method::kGet;
        args.expiry_seconds   = static_cast<unsigned int>(opts_.presignTtl.count());

        // attachment 모드면 response-content-disposition 쿼리 파라미터로 강제.
        // (S3/MinIO presigned URL이 직접 Content-Disposition을 override 할 수 있도록)
        if (disposition == "attachment" && !filename.empty()) {
            std::string cd = "attachment; filename=\"" + filename + "\"";
            args.extra_query_params.Add("response-content-disposition", cd);
        }

        auto presigned = impl_->client->GetPresignedObjectUrl(args);
        if (!presigned) {
            std::cerr << "[minio] presigned failed: "
                      << presigned.Error().String() << std::endl;
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            return resp;
        }
        return drogon::HttpResponse::newRedirectionResponse(presigned.url, drogon::k302Found);
    } catch (const std::exception& e) {
        std::cerr << "[minio] serve error: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k500InternalServerError);
        return resp;
    }
}

} // namespace monggle
