#pragma once

#include <cstdint>
#include <string_view>

namespace monggle::permissions {

// 기획 8장 권한 매트릭스. DB/네트워크 의존성 없는 순수 함수로 분리해
// 단위 테스트(GoogleTest)에서 모든 조합을 검증할 수 있게 한다.
//
// visibility:    "public" | "friends" | "private"
// downloadPolicy: "owner_only" | "followers" | "public_allowed"

// 글을 볼 수 있는가?
//   - 본인이면 항상 true
//   - 차단된 사용자라면 false (visibility 무관)
//   - public  : 모두 true
//   - friends : 팔로워만 true
//   - private : 본인 외 false
bool canView(std::int64_t viewerId,
             std::int64_t authorId,
             std::string_view visibility,
             bool isFollowerOfAuthor,
             bool isBlockedByAuthor);

// 다운로드 가능한가?
//   - canView가 false면 자동 false
//   - 본인이면 항상 true
//   - owner_only       : 본인만
//   - followers        : 팔로워만
//   - public_allowed   : visibility=="public"일 때만
bool canDownload(std::int64_t viewerId,
                 std::int64_t authorId,
                 std::string_view visibility,
                 std::string_view downloadPolicy,
                 bool isFollowerOfAuthor,
                 bool isBlockedByAuthor);

} // namespace monggle::permissions
