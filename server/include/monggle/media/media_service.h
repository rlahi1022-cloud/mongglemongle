#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace monggle {

class FollowsService;

enum class MediaKind { Photo, Video };

struct MediaAsset {
    std::int64_t id;
    std::int64_t postId;
    std::int64_t userId;
    MediaKind    kind;
    std::string  storeKeyOriginal;   // 로컬 상대경로 (TODO: S3 마이그 시 s3://)
    std::string  storeKeyThumb;      // 사진만
    std::string  storeKeyPoster;     // 영상만
    std::string  mimeType;
    std::int64_t sizeBytes;
    int          widthPx;
    int          heightPx;
    int          durationMs;
    std::string  status;             // 'pending'|'ready'|'failed'
    std::string  createdAt;
};

struct MediaError {
    enum Code { NotFound, Forbidden, BadRequest, UnsupportedType, InternalError };
    Code        code;
    std::string detail;
};

template <typename T>
using MResult = std::variant<T, MediaError>;

// 어디까지 보여줄지/다운로드 할지 결정. PostsService::canView 와 같은 규칙 + download_policy 추가.
struct ViewerContext {
    std::int64_t viewerId;           // -1 = 익명
    bool         isFollowerOfAuthor; // 호출자가 미리 계산해 넘김
};

class MediaService {
public:
    // 미디어 저장 루트 (./media 또는 환경변수). 없으면 자동 생성.
    explicit MediaService(std::string storageRoot,
                          std::shared_ptr<FollowsService> follows = nullptr);

    // multipart upload — 한 번의 요청으로 메타+바이트 모두 전달.
    // postId 기준 권한 검사: postId의 owner == authorId 여야 함.
    MResult<MediaAsset> uploadForPost(std::int64_t authorId,
                                      std::int64_t postId,
                                      const std::string& filename,
                                      const std::string& mimeType,
                                      const char*       bytes,
                                      std::size_t       size);

    // 메타 조회 + 권한 체크. URL 라우터에서 사용.
    MResult<MediaAsset> getForView(const ViewerContext& vc, std::int64_t mediaId);

    // 다운로드 권한 체크 — 별도 매트릭스 (기획 8.3)
    MResult<MediaAsset> getForDownload(const ViewerContext& vc, std::int64_t mediaId);

    // 글에 첨부된 미디어 목록 (피드/타임라인 카드 미리보기용)
    MResult<std::vector<MediaAsset>> listForPost(const ViewerContext& vc, std::int64_t postId);

private:
    std::string storageRoot_;
    std::shared_ptr<FollowsService> follows_;
};

} // namespace monggle
