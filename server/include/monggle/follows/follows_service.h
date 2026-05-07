#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace monggle {

struct FollowError {
    enum Code { NotFound, AlreadyFollowing, SelfFollow, BadRequest, InternalError };
    Code        code;
    std::string detail;
};

template <typename T>
using FResult = std::variant<T, FollowError>;

struct UserBrief {
    std::int64_t id;
    std::string  email;
    std::string  displayName;
};

class FollowsService {
public:
    FResult<bool> follow(std::int64_t followerId, std::int64_t followeeId);
    FResult<bool> unfollow(std::int64_t followerId, std::int64_t followeeId);

    bool isFollower(std::int64_t followerId, std::int64_t followeeId);

    FResult<std::vector<UserBrief>> listFollowers(std::int64_t userId, int limit);
    FResult<std::vector<UserBrief>> listFollowing(std::int64_t userId, int limit);

    // /me/feed 핵심 — viewerId가 본인 + 팔로우한 사람의 글 (visibility 필터 포함)
    // Pull 전략 (작성 시 fanout 없음). 팔로워 폭증 시 Push 도입은 C6.
    struct FeedItem {
        std::int64_t id;
        std::int64_t userId;
        std::string  authorDisplayName;
        std::string  body;
        std::string  visibility;
        std::string  downloadPolicy;
        std::string  createdAt;
        std::string  updatedAt;
    };
    struct FeedPage {
        std::vector<FeedItem> items;
        std::int64_t nextCursor;
        bool hasNext;
    };
    FResult<FeedPage> feed(std::int64_t viewerId, std::int64_t cursor, int limit);
};

} // namespace monggle
