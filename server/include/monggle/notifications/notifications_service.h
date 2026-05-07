#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace monggle {

struct NotificationItem {
    std::int64_t id;
    std::int64_t userId;
    std::string  kind;             // 'follow' | 'comment'
    std::optional<std::int64_t> actorId;
    std::optional<std::int64_t> targetId;
    std::string  body;
    bool         isRead;
    std::string  createdAt;
    std::string  actorName;        // join from users (없으면 빈 문자열)
};

class NotificationsService {
public:
    // 자기 자신에게 보내는 알림은 호출자가 거르도록 함 (여기선 무조건 INSERT)
    void enqueue(std::int64_t recipientId,
                 const std::string& kind,
                 std::optional<std::int64_t> actorId,
                 std::optional<std::int64_t> targetId,
                 const std::string& body);

    std::vector<NotificationItem> recent(std::int64_t userId, int limit);
    int  unreadCount(std::int64_t userId);
    bool markAllRead(std::int64_t userId);
};

} // namespace monggle
