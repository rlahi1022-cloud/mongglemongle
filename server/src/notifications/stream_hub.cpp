#include "monggle/notifications/stream_hub.h"

#include <algorithm>

namespace monggle {

NotificationStreamHub& NotificationStreamHub::instance() {
    static NotificationStreamHub inst;
    return inst;
}

std::shared_ptr<NotificationStreamHub::Stream>
NotificationStreamHub::registerStream(std::int64_t userId) {
    auto s = std::make_shared<Stream>();
    s->userId = userId;
    std::lock_guard<std::mutex> lk(mu_);
    byUser_[userId].push_back(s);
    return s;
}

void NotificationStreamHub::publish(std::int64_t userId, const std::string& jsonPayload) {
    const std::string sse = "data: " + jsonPayload + "\n\n";

    std::vector<std::weak_ptr<Stream>> snapshot;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = byUser_.find(userId);
        if (it == byUser_.end()) return;
        snapshot = it->second;
    }

    bool shouldGc = false;
    for (auto& w : snapshot) {
        auto s = w.lock();
        if (!s) { shouldGc = true; continue; }
        std::lock_guard<std::mutex> lk2(s->mu);
        if (s->closed) { shouldGc = true; continue; }
        s->queue.push_back(sse);
        s->cv.notify_all();
    }

    if (shouldGc) {
        std::lock_guard<std::mutex> lk(mu_);
        auto& vec = byUser_[userId];
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [](const std::weak_ptr<Stream>& w) {
                                     auto s = w.lock();
                                     return !s || s->closed;
                                 }),
                  vec.end());
        if (vec.empty()) byUser_.erase(userId);
    }
}

} // namespace monggle
