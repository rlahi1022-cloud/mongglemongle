#include "monggle/comments/comments_service.h"
#include "monggle/follows/follows_service.h"
#include "monggle/notifications/notifications_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

namespace monggle {

namespace {

drogon::orm::DbClientPtr db() { return drogon::app().getDbClient("monggle_db"); }

struct PostMetaSlim {
    std::int64_t userId;
    std::string  visibility;
    bool         deleted;
    std::string  title;
    std::string  body;
};

std::optional<PostMetaSlim> loadPost(std::int64_t postId) {
    auto rows = db()->execSqlSync(
        "SELECT user_id, visibility, deleted_at, title, body "
        "FROM posts WHERE id = ? LIMIT 1", postId);
    if (rows.size() == 0) return std::nullopt;
    PostMetaSlim m;
    m.userId     = rows[0]["user_id"].as<std::int64_t>();
    m.visibility = rows[0]["visibility"].as<std::string>();
    m.deleted    = !rows[0]["deleted_at"].isNull();
    m.title      = rows[0]["title"].isNull() ? std::string{} : rows[0]["title"].as<std::string>();
    m.body       = rows[0]["body"].as<std::string>();
    return m;
}

bool canView(std::int64_t viewerId, const PostMetaSlim& p, FollowsService* follows) {
    if (viewerId == p.userId) return true;
    if (p.visibility == "public")  return true;
    if (p.visibility == "friends") return follows && viewerId > 0
                                          && follows->isFollower(viewerId, p.userId);
    return false;
}

Comment rowToComment(const drogon::orm::Row& r) {
    Comment c;
    c.id         = r["id"].as<std::int64_t>();
    c.postId     = r["post_id"].as<std::int64_t>();
    c.userId     = r["user_id"].as<std::int64_t>();
    c.body       = r["body"].as<std::string>();
    c.authorName = r["author_name"].as<std::string>();
    c.createdAt  = r["created_at"].as<std::string>();
    return c;
}

} // namespace

CommentsService::CommentsService(std::shared_ptr<FollowsService> follows,
                                  std::shared_ptr<NotificationsService> notifications)
    : follows_(std::move(follows)), notif_(std::move(notifications)) {}

CResult<Comment> CommentsService::create(std::int64_t userId,
                                          std::int64_t postId,
                                          const std::string& body) {
    if (body.empty())     return CommentError{CommentError::BadRequest, "body required"};
    if (body.size() > 2000) return CommentError{CommentError::BadRequest, "body too long"};
    try {
        auto p = loadPost(postId);
        if (!p || p->deleted) return CommentError{CommentError::NotFound, "post not found"};
        if (!canView(userId, *p, follows_.get())) {
            return CommentError{CommentError::Forbidden, "no permission"};
        }

        auto inserted = db()->execSqlSync(
            "INSERT INTO comments (post_id, user_id, body) VALUES (?, ?, ?)",
            postId, userId, body);
        std::int64_t commentId = inserted.insertId();

        auto rows = db()->execSqlSync(
            "SELECT c.id, c.post_id, c.user_id, c.body, c.created_at, "
            "       u.display_name AS author_name "
            "FROM comments c JOIN users u ON u.id = c.user_id "
            "WHERE c.id = ?", commentId);
        if (rows.size() == 0) {
            return CommentError{CommentError::InternalError, "comment vanished"};
        }
        Comment cm = rowToComment(rows[0]);

        // 자기 글이 아니면 알림 enqueue
        if (notif_ && p->userId != userId) {
            std::string preview = cm.body.size() > 80 ? cm.body.substr(0, 80) + "..." : cm.body;
            notif_->enqueue(p->userId, "comment", userId, postId,
                            cm.authorName + ": " + preview);
        }
        return cm;
    } catch (const drogon::orm::DrogonDbException& e) {
        return CommentError{CommentError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return CommentError{CommentError::InternalError, e.what()};
    }
}

CResult<std::vector<Comment>> CommentsService::listForPost(std::int64_t viewerId, std::int64_t postId) {
    try {
        auto p = loadPost(postId);
        if (!p || p->deleted) return CommentError{CommentError::NotFound, "post not found"};
        if (!canView(viewerId, *p, follows_.get())) {
            return CommentError{CommentError::Forbidden, "no permission"};
        }
        auto rows = db()->execSqlSync(
            "SELECT c.id, c.post_id, c.user_id, c.body, c.created_at, "
            "       u.display_name AS author_name "
            "FROM comments c JOIN users u ON u.id = c.user_id "
            "WHERE c.post_id = ? AND c.deleted_at IS NULL "
            "ORDER BY c.id ASC", postId);
        std::vector<Comment> out;
        out.reserve(rows.size());
        for (const auto& r : rows) out.push_back(rowToComment(r));
        return out;
    } catch (const std::exception& e) {
        return CommentError{CommentError::InternalError, e.what()};
    }
}

CResult<bool> CommentsService::remove(std::int64_t userId, std::int64_t commentId) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT user_id FROM comments WHERE id = ? AND deleted_at IS NULL LIMIT 1",
            commentId);
        if (rows.size() == 0) return CommentError{CommentError::NotFound, "comment not found"};
        if (rows[0]["user_id"].as<std::int64_t>() != userId) {
            return CommentError{CommentError::Forbidden, "not your comment"};
        }
        db()->execSqlSync(
            "UPDATE comments SET deleted_at = NOW(3) WHERE id = ?", commentId);
        return true;
    } catch (const std::exception& e) {
        return CommentError{CommentError::InternalError, e.what()};
    }
}

} // namespace monggle
