#include "monggle/blocks/blocks_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

namespace monggle {

namespace {
drogon::orm::DbClientPtr db() { return drogon::app().getDbClient("monggle_db"); }
} // namespace

BResult<bool> BlocksService::block(std::int64_t blockerId, std::int64_t blockedId) {
    if (blockerId == blockedId)
        return BlockError{BlockError::SelfBlock, "cannot block yourself"};
    try {
        auto exists = db()->execSqlSync(
            "SELECT 1 FROM users WHERE id = ? LIMIT 1", blockedId);
        if (exists.size() == 0)
            return BlockError{BlockError::NotFound, "target user not found"};

        auto already = db()->execSqlSync(
            "SELECT 1 FROM blocks WHERE blocker_id = ? AND blocked_id = ? LIMIT 1",
            blockerId, blockedId);
        if (already.size() > 0)
            return BlockError{BlockError::AlreadyBlocked, "already blocked"};

        auto tx = db()->newTransaction();
        tx->execSqlSync(
            "INSERT INTO blocks (blocker_id, blocked_id) VALUES (?, ?)",
            blockerId, blockedId);
        // 양방향 follow 자동 해제 (있으면)
        tx->execSqlSync(
            "DELETE FROM follows WHERE (follower_id=? AND followee_id=?) OR (follower_id=? AND followee_id=?)",
            blockerId, blockedId, blockedId, blockerId);
        return true;
    } catch (const drogon::orm::DrogonDbException& e) {
        return BlockError{BlockError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return BlockError{BlockError::InternalError, e.what()};
    }
}

BResult<bool> BlocksService::unblock(std::int64_t blockerId, std::int64_t blockedId) {
    try {
        auto r = db()->execSqlSync(
            "DELETE FROM blocks WHERE blocker_id = ? AND blocked_id = ?",
            blockerId, blockedId);
        if (r.affectedRows() == 0)
            return BlockError{BlockError::NotFound, "not blocked"};
        return true;
    } catch (const std::exception& e) {
        return BlockError{BlockError::InternalError, e.what()};
    }
}

bool BlocksService::isBlocked(std::int64_t a, std::int64_t b) {
    if (a <= 0 || b <= 0 || a == b) return false;
    try {
        auto rows = db()->execSqlSync(
            "SELECT 1 FROM blocks WHERE "
            "(blocker_id=? AND blocked_id=?) OR (blocker_id=? AND blocked_id=?) LIMIT 1",
            a, b, b, a);
        return rows.size() > 0;
    } catch (...) { return false; }
}

BResult<std::vector<BlockedUser>> BlocksService::list(std::int64_t userId, int limit) {
    if (limit <= 0 || limit > 200) limit = 100;
    try {
        auto rows = db()->execSqlSync(
            "SELECT u.id, u.email, u.display_name, b.created_at "
            "FROM blocks b JOIN users u ON u.id = b.blocked_id "
            "WHERE b.blocker_id = ? "
            "ORDER BY b.created_at DESC LIMIT ?",
            userId, limit);
        std::vector<BlockedUser> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back({
                r["id"].as<std::int64_t>(),
                r["email"].as<std::string>(),
                r["display_name"].as<std::string>(),
                r["created_at"].as<std::string>(),
            });
        }
        return out;
    } catch (const std::exception& e) {
        return BlockError{BlockError::InternalError, e.what()};
    }
}

} // namespace monggle
