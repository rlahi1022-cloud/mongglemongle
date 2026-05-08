#include "monggle/cache/layered_cache.h"
#include "monggle/metrics/metrics.h"

#include <algorithm>
#include <memory>
#include <mutex>

namespace monggle {

namespace {

struct FeedSlot {
    std::mutex mu;
    std::shared_ptr<LayeredCache> instance;
};

FeedSlot& feedSlot() {
    static FeedSlot s;
    return s;
}

} // namespace

LayeredCache::LayeredCache(std::shared_ptr<TtlCache> l1, std::shared_ptr<RedisCache> l2)
    : l1_(std::move(l1)), l2_(std::move(l2)) {}

std::optional<std::string> LayeredCache::get(const std::string& key) {
    auto& m = Metrics::instance();
    if (l1_) {
        if (auto v = l1_->get(key)) {
            m.incCounter("cache_hits_total", {{"layer", "l1"}});
            return v;
        }
    }
    if (l2_ && l2_->healthy()) {
        if (auto v = l2_->get(key)) {
            // L2 hit는 L1에 짧게 채워서 다음 호출의 hot path를 빠르게.
            if (l1_) l1_->set(key, *v, l1Cap_);
            m.incCounter("cache_hits_total", {{"layer", "l2"}});
            return v;
        }
    }
    m.incCounter("cache_misses_total", {});
    return std::nullopt;
}

void LayeredCache::set(const std::string& key, std::string value, std::chrono::seconds ttl) {
    auto l1Ttl = std::min(ttl, l1Cap_);
    if (l1_) l1_->set(key, value, l1Ttl);
    if (l2_ && l2_->healthy()) l2_->set(key, std::move(value), ttl);
}

void LayeredCache::invalidate(const std::string& key) {
    if (l1_) l1_->invalidate(key);
    if (l2_ && l2_->healthy()) l2_->invalidate(key);
}

std::size_t LayeredCache::invalidatePrefix(const std::string& prefix) {
    std::size_t n = 0;
    if (l1_) n += l1_->invalidatePrefix(prefix);
    if (l2_ && l2_->healthy()) n += l2_->invalidatePrefix(prefix);
    return n;
}

bool LayeredCache::healthy() const {
    return l1_ != nullptr; // L1만 살아있어도 캐시는 동작
}

bool LayeredCache::l2Healthy() const {
    return l2_ && l2_->healthy();
}

LayeredCache& LayeredCache::feed() {
    auto& slot = feedSlot();
    std::lock_guard<std::mutex> lk(slot.mu);
    if (!slot.instance) {
        // initFeed()가 호출되지 않은 환경 (테스트 등)에서는 L1 only로 동작.
        slot.instance = std::make_shared<LayeredCache>(
            std::make_shared<TtlCache>(), nullptr);
    }
    return *slot.instance;
}

void LayeredCache::initFeed(std::shared_ptr<TtlCache> l1, std::shared_ptr<RedisCache> l2) {
    auto& slot = feedSlot();
    std::lock_guard<std::mutex> lk(slot.mu);
    slot.instance = std::make_shared<LayeredCache>(std::move(l1), std::move(l2));
}

} // namespace monggle
