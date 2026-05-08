#include "monggle/notifications/notifications_service.h"
#include "monggle/notifications/stream_hub.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <sstream>
#include <trantor/utils/Logger.h>

namespace monggle {

namespace {
drogon::orm::DbClientPtr db() { return drogon::app().getDbClient("monggle_db"); }
} // namespace

void NotificationsService::enqueue(std::int64_t recipientId,
                                   const std::string& kind,
                                   std::optional<std::int64_t> actorId,
                                   std::optional<std::int64_t> targetId,
                                   const std::string& body) {
    try {
        if (actorId && targetId) {
            db()->execSqlSync(
                "INSERT INTO notifications (user_id, kind, actor_id, target_id, body) "
                "VALUES (?, ?, ?, ?, ?)",
                recipientId, kind, *actorId, *targetId, body);
        } else if (actorId) {
            db()->execSqlSync(
                "INSERT INTO notifications (user_id, kind, actor_id, body) "
                "VALUES (?, ?, ?, ?)",
                recipientId, kind, *actorId, body);
        } else {
            db()->execSqlSync(
                "INSERT INTO notifications (user_id, kind, body) VALUES (?, ?, ?)",
                recipientId, kind, body);
        }
    } catch (const std::exception& e) {
        LOG_WARN << "[notifications] enqueue failed: " << e.what();
        return;
    }

    // SSE 스트림으로 즉시 푸시. queue 기반이라 publish는 non-blocking.
    std::ostringstream js;
    js << "{\"kind\":\"" << kind << "\",\"body\":\"";
    for (char c : body) {
        if (c == '"' || c == '\\') js << '\\';
        if (c == '\n') js << "\\n";
        else js << c;
    }
    js << "\"";
    if (actorId)  js << ",\"actor_id\":"  << *actorId;
    if (targetId) js << ",\"target_id\":" << *targetId;
    js << "}";
    NotificationStreamHub::instance().publish(recipientId, js.str());
}

std::vector<NotificationItem> NotificationsService::recent(std::int64_t userId, int limit) {
    if (limit <= 0 || limit > 100) limit = 30;
    std::vector<NotificationItem> out;
    try {
        auto rows = db()->execSqlSync(
            "SELECT n.id, n.user_id, n.kind, n.actor_id, n.target_id, "
            "       n.body, n.read_at, n.created_at, "
            "       COALESCE(u.display_name, '') AS actor_name "
            "FROM notifications n "
            "LEFT JOIN users u ON u.id = n.actor_id "
            "WHERE n.user_id = ? "
            "ORDER BY n.id DESC LIMIT ?",
            userId, limit);
        out.reserve(rows.size());
        for (const auto& r : rows) {
            NotificationItem n;
            n.id        = r["id"].as<std::int64_t>();
            n.userId    = r["user_id"].as<std::int64_t>();
            n.kind      = r["kind"].as<std::string>();
            if (!r["actor_id"].isNull())  n.actorId  = r["actor_id"].as<std::int64_t>();
            if (!r["target_id"].isNull()) n.targetId = r["target_id"].as<std::int64_t>();
            n.body      = r["body"].isNull() ? std::string{} : r["body"].as<std::string>();
            n.isRead    = !r["read_at"].isNull();
            n.createdAt = r["created_at"].as<std::string>();
            n.actorName = r["actor_name"].as<std::string>();
            out.push_back(std::move(n));
        }
    } catch (...) {}
    return out;
}

int NotificationsService::unreadCount(std::int64_t userId) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT COUNT(*) AS c FROM notifications "
            "WHERE user_id = ? AND read_at IS NULL", userId);
        if (rows.size() == 0) return 0;
        return rows[0]["c"].as<int>();
    } catch (...) { return 0; }
}

bool NotificationsService::markAllRead(std::int64_t userId) {
    try {
        db()->execSqlSync(
            "UPDATE notifications SET read_at = NOW(3) "
            "WHERE user_id = ? AND read_at IS NULL", userId);
        return true;
    } catch (...) { return false; }
}

} // namespace monggle
