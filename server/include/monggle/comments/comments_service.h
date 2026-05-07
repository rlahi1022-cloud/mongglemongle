#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace monggle {

class FollowsService;
class NotificationsService;

struct Comment {
    std::int64_t id;
    std::int64_t postId;
    std::int64_t userId;
    std::string  body;
    std::string  authorName;   // join
    std::string  createdAt;
};

struct CommentError {
    enum Code { NotFound, Forbidden, BadRequest, InternalError };
    Code code;
    std::string detail;
};

template <typename T>
using CResult = std::variant<T, CommentError>;

class CommentsService {
public:
    CommentsService(std::shared_ptr<FollowsService> follows,
                    std::shared_ptr<NotificationsService> notifications);

    // 권한: post를 볼 수 있어야 댓글 가능 (visibility + follower)
    CResult<Comment> create(std::int64_t userId, std::int64_t postId, const std::string& body);
    CResult<std::vector<Comment>> listForPost(std::int64_t viewerId, std::int64_t postId);
    CResult<bool> remove(std::int64_t userId, std::int64_t commentId);

private:
    std::shared_ptr<FollowsService>       follows_;
    std::shared_ptr<NotificationsService> notif_;
};

} // namespace monggle
