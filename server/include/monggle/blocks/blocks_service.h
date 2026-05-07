#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace monggle {

struct BlockedUser {
    std::int64_t id;
    std::string  email;
    std::string  displayName;
    std::string  blockedAt;
};

struct BlockError {
    enum Code { NotFound, SelfBlock, AlreadyBlocked, BadRequest, InternalError };
    Code        code;
    std::string detail;
};

template <typename T>
using BResult = std::variant<T, BlockError>;

class BlocksService {
public:
    // 차단: 양방향 follow 자동 해제 + 신규 follow 막힘 (FollowsService에서 검사)
    BResult<bool> block(std::int64_t blockerId, std::int64_t blockedId);
    BResult<bool> unblock(std::int64_t blockerId, std::int64_t blockedId);

    // 어느 한쪽이라도 차단했으면 true (피드/댓글/follow 게이트용)
    bool isBlocked(std::int64_t a, std::int64_t b);

    BResult<std::vector<BlockedUser>> list(std::int64_t userId, int limit);
};

} // namespace monggle
