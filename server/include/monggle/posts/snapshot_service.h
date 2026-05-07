#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace monggle {

// 임의 시점 t에서의 post 상태 — id별 본문/visibility/삭제여부
struct PostStateAt {
    std::int64_t id;
    std::string  body;
    std::string  visibility;       // 'public'|'friends'|'private'
    bool         deleted;
    std::int64_t lastEventId;      // 이 시점까지 반영된 마지막 post_events.id
};

struct UserStateAt {
    std::int64_t userId;
    std::string  targetTime;       // 사용자가 요청한 시점 (echo)
    std::vector<PostStateAt> posts;
};

struct SnapshotError {
    enum Code { BadRequest, InternalError };
    Code        code;
    std::string detail;
};

template <typename T>
using SnapResult = std::variant<T, SnapshotError>;

// 기획 10.5 — 스냅샷 + 이벤트 재생.
// MVP: snapshots 테이블이 비어있어도 동작 (이벤트 처음부터 재생).
class SnapshotService {
public:
    // targetTimeIso = "YYYY-MM-DD HH:MM:SS" 또는 "YYYY-MM-DDTHH:MM:SS" (UTC)
    SnapResult<UserStateAt> restoreState(std::int64_t userId,
                                         const std::string& targetTimeIso);
};

} // namespace monggle
