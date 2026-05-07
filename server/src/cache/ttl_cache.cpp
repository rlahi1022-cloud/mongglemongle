#include "monggle/cache/ttl_cache.h"

namespace monggle {

std::optional<std::string> TtlCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;
    if (std::chrono::steady_clock::now() > it->second.expiresAt) {
        map_.erase(it);
        return std::nullopt;
    }
    return it->second.value;
}

void TtlCache::set(const std::string& key, std::string value, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lk(mu_);
    map_[key] = Entry{
        std::move(value),
        std::chrono::steady_clock::now() + ttl,
    };
}

void TtlCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lk(mu_);
    map_.erase(key);
}

std::size_t TtlCache::invalidatePrefix(const std::string& prefix) {
    std::lock_guard<std::mutex> lk(mu_);
    std::size_t n = 0;
    for (auto it = map_.begin(); it != map_.end();) {
        if (it->first.rfind(prefix, 0) == 0) {
            it = map_.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    return n;
}

std::size_t TtlCache::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return map_.size();
}

TtlCache& TtlCache::feedCache() {
    static TtlCache inst;
    return inst;
}

} // namespace monggle
