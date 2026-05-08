#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace drogon { class HttpResponse; }

namespace monggle {

// Backend-agnostic media object storage.
//   - LocalFsStorage: ./media 루트 아래 상대 경로로 저장. 단일 노드 개발/테스트용.
//   - MinioStorage:   S3 호환 버킷에 저장. 운영 + 멀티 노드.
// MediaService는 이 인터페이스만 보고, 라우터는 serve()로 응답을 받는다.
class IMediaStorage {
public:
    virtual ~IMediaStorage() = default;

    // 로컬 파일을 key로 업로드 (OpenCV/ffmpeg가 만들어낸 결과물 등).
    // mimeType은 비어있으면 application/octet-stream.
    // 반환: 업로드 성공 여부.
    virtual bool putFile(const std::string& key,
                         const std::string& localPath,
                         const std::string& mimeType) = 0;

    // 메모리 바이트를 key로 업로드.
    virtual bool putBytes(const std::string& key,
                          const char* data,
                          std::size_t size,
                          const std::string& mimeType) = 0;

    // key의 객체를 로컬 임시 파일로 다운로드. OpenCV/ffmpeg 처리 필요 시 사용.
    // 반환: 다운로드된 로컬 경로 (실패 시 nullopt).
    virtual std::optional<std::string> fetchToTemp(const std::string& key) = 0;

    // 객체 삭제. 존재하지 않아도 true 반환.
    virtual bool deleteObject(const std::string& key) = 0;

    // HTTP 응답 빌드 — 라우터의 view/download/thumb 경로에서 사용.
    // disposition이 "attachment"면 Content-Disposition 헤더 추가.
    // 구현체별 동작:
    //   - LocalFs: setBody + 파일 read
    //   - Minio:   presigned URL로 302 redirect (TTL 5분)
    virtual std::shared_ptr<drogon::HttpResponse>
    serve(const std::string& key,
          const std::string& mimeType,
          const std::string& disposition = "",
          const std::string& filename = "") = 0;

    // 진단용 — 어떤 백엔드인지 알리기 위해.
    virtual std::string backendName() const = 0;
};

} // namespace monggle
