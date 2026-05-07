#include "monggle/event/event_bus.h"

#include <trantor/utils/Logger.h>

namespace monggle {

void EventBus::subscribe(const std::string& topic, Handler h) {
    std::lock_guard<std::mutex> lk(mu_);
    subs_[topic].push_back(std::move(h));
}

void EventBus::publish(const std::string& topic, const std::string& payload) {
    std::vector<Handler> snapshot;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = subs_.find(topic);
        if (it != subs_.end()) snapshot = it->second;
    }
    published_.fetch_add(1, std::memory_order_relaxed);
    for (auto& h : snapshot) {
        try { h(payload); }
        catch (const std::exception& e) {
            LOG_WARN << "[EventBus] handler for " << topic << " threw: " << e.what();
        }
    }
}

} // namespace monggle
