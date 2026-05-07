#include "monggle/follows/follows_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

namespace monggle {

namespace {

drogon::orm::DbClientPtr db() {
    return drogon::app().getDbClient("monggle_db");
}

bool isBlockedPair(std::int64_t a, std::int64_t b) {
    if (a <= 0 || b <= 0 || a == b) return false;
    auto rows = db()->execSqlSync(
        "SELECT 1 FROM blocks WHERE "
        "(blocker_id=? AND blocked_id=?) OR (blocker_id=? AND blocked_id=?) LIMIT 1",
        a, b, b, a);
    return rows.size() > 0;
}

} // namespace

FResult<bool> FollowsService::follow(std::int64_t followerId, std::int64_t followeeId) {
    if (followerId == followeeId) {
        return FollowError{FollowError::SelfFollow, "cannot follow yourself"};
    }
    try {
        auto exists = db()->execSqlSync(
            "SELECT 1 FROM users WHERE id = ? LIMIT 1", followeeId);
        if (exists.size() == 0) {
            return FollowError{FollowError::NotFound, "target user not found"};
        }

        if (isBlockedPair(followerId, followeeId)) {
            return FollowError{FollowError::BadRequest, "blocked users cannot follow each other"};
        }

        auto already = db()->execSqlSync(
            "SELECT 1 FROM follows WHERE follower_id = ? AND followee_id = ? LIMIT 1",
            followerId, followeeId);
        if (already.size() > 0) {
            return FollowError{FollowError::AlreadyFollowing, "already following"};
        }

        db()->execSqlSync(
            "INSERT INTO follows (follower_id, followee_id) VALUES (?, ?)",
            followerId, followeeId);
        return true;
    } catch (const drogon::orm::DrogonDbException& e) {
        return FollowError{FollowError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return FollowError{FollowError::InternalError, e.what()};
    }
}

FResult<bool> FollowsService::unfollow(std::int64_t followerId, std::int64_t followeeId) {
    try {
        auto r = db()->execSqlSync(
            "DELETE FROM follows WHERE follower_id = ? AND followee_id = ?",
            followerId, followeeId);
        if (r.affectedRows() == 0) {
            return FollowError{FollowError::NotFound, "not following"};
        }
        return true;
    } catch (const std::exception& e) {
        return FollowError{FollowError::InternalError, e.what()};
    }
}

bool FollowsService::isFollower(std::int64_t followerId, std::int64_t followeeId) {
    if (followerId <= 0 || followeeId <= 0) return false;
    if (followerId == followeeId) return true;
    try {
        auto rows = db()->execSqlSync(
            "SELECT 1 FROM follows WHERE follower_id = ? AND followee_id = ? LIMIT 1",
            followerId, followeeId);
        return rows.size() > 0;
    } catch (...) {
        return false;
    }
}

FResult<std::vector<UserBrief>> FollowsService::listFollowers(std::int64_t userId, int limit) {
    if (limit <= 0 || limit > 200) limit = 50;
    try {
        auto rows = db()->execSqlSync(
            "SELECT u.id, u.email, u.display_name "
            "FROM follows f JOIN users u ON u.id = f.follower_id "
            "WHERE f.followee_id = ? "
            "  AND u.id NOT IN (SELECT blocked_id FROM blocks WHERE blocker_id = ?) "
            "  AND u.id NOT IN (SELECT blocker_id FROM blocks WHERE blocked_id = ?) "
            "ORDER BY f.created_at DESC LIMIT ?",
            userId, userId, userId, limit);
        std::vector<UserBrief> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back({
                r["id"].as<std::int64_t>(),
                r["email"].as<std::string>(),
                r["display_name"].as<std::string>()
            });
        }
        return out;
    } catch (const std::exception& e) {
        return FollowError{FollowError::InternalError, e.what()};
    }
}

FResult<std::vector<UserBrief>> FollowsService::listFollowing(std::int64_t userId, int limit) {
    if (limit <= 0 || limit > 200) limit = 50;
    try {
        auto rows = db()->execSqlSync(
            "SELECT u.id, u.email, u.display_name "
            "FROM follows f JOIN users u ON u.id = f.followee_id "
            "WHERE f.follower_id = ? "
            "  AND u.id NOT IN (SELECT blocked_id FROM blocks WHERE blocker_id = ?) "
            "  AND u.id NOT IN (SELECT blocker_id FROM blocks WHERE blocked_id = ?) "
            "ORDER BY f.created_at DESC LIMIT ?",
            userId, userId, userId, limit);
        std::vector<UserBrief> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back({
                r["id"].as<std::int64_t>(),
                r["email"].as<std::string>(),
                r["display_name"].as<std::string>()
            });
        }
        return out;
    } catch (const std::exception& e) {
        return FollowError{FollowError::InternalError, e.what()};
    }
}

FResult<FollowsService::FeedPage> FollowsService::feed(std::int64_t viewerId,
                                                        std::int64_t cursor,
                                                        int limit) {
    if (limit <= 0 || limit > 100) limit = 30;
    const int fetchN = limit + 1;
    try {
        // Pull 전략 — 본인 글 OR 팔로우한 사람의 (public|friends) 글
        // friends visibility는 follower 관계만 통과
        std::string sql =
            "SELECT p.id, p.user_id, p.title, p.category, p.body, p.visibility, p.download_policy, "
            "       p.created_at, p.updated_at, u.display_name AS author_name "
            "FROM posts p JOIN users u ON u.id = p.user_id "
            "WHERE p.deleted_at IS NULL "
            // 양방향 차단 필터 — 본인이 차단했거나 본인을 차단한 사용자의 글 제외
            "  AND p.user_id NOT IN (SELECT blocked_id FROM blocks WHERE blocker_id = ?) "
            "  AND p.user_id NOT IN (SELECT blocker_id FROM blocks WHERE blocked_id = ?) "
            "  AND ( "
            "    p.user_id = ? "
            "    OR ( p.user_id IN (SELECT followee_id FROM follows WHERE follower_id = ?) "
            "         AND p.visibility IN ('public','friends') ) "
            "    OR p.visibility = 'public' "
            "  ) ";
        if (cursor > 0) sql += " AND p.id < ? ";
        sql += " ORDER BY p.id DESC LIMIT ?";

        drogon::orm::Result rows = (cursor > 0)
            ? db()->execSqlSync(sql, viewerId, viewerId, viewerId, viewerId, cursor, fetchN)
            : db()->execSqlSync(sql, viewerId, viewerId, viewerId, viewerId, fetchN);

        FeedPage page;
        for (const auto& r : rows) {
            FeedItem it;
            it.id                  = r["id"].as<std::int64_t>();
            it.userId              = r["user_id"].as<std::int64_t>();
            it.authorDisplayName   = r["author_name"].as<std::string>();
            it.title               = r["title"].isNull() ? std::string{} : r["title"].as<std::string>();
            it.category            = r["category"].as<std::string>();
            it.body                = r["body"].as<std::string>();
            it.visibility          = r["visibility"].as<std::string>();
            it.downloadPolicy      = r["download_policy"].as<std::string>();
            it.createdAt           = r["created_at"].as<std::string>();
            it.updatedAt           = r["updated_at"].as<std::string>();
            page.items.push_back(it);
        }
        page.hasNext = static_cast<int>(page.items.size()) > limit;
        if (page.hasNext) {
            page.nextCursor = page.items[limit - 1].id;
            page.items.resize(limit);
        } else {
            page.nextCursor = 0;
        }
        return page;
    } catch (const std::exception& e) {
        return FollowError{FollowError::InternalError, e.what()};
    }
}

} // namespace monggle
