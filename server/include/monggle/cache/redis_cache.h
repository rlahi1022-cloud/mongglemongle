#pragma once

#include "monggle/cache/icache.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace sw { namespace redis { class Redis; } }

namespace monggle {

// L2: redis-plus-plus 기반 Redis 캐시.
// 모든 메서드는 fail-soft — Redis가 다운되면 nullopt/0을 반환하고 healthy_=false로 떨어진다.
// 캐시는 정합성 critical하지 않기 때문에 LayeredCache는 L1만으로 계속 동작한다.
class RedisCache : public ICache {
public:
    struct Options {
        std::string host = "127.0.0.1";
        std::uint16_t port = 6379;
        std::size_t poolSize = 4;
        std::chrono::milliseconds connectTimeout{300};
        std::chrono::milliseconds socketTimeout{300};
    };

    explicit RedisCache(const Options& opts);
    ~RedisCache() override;

    std::optional<std::string> get(const std::string& key) override;
    void set(const std::string& key, std::string value, std::chrono::seconds ttl) override;
    void invalidate(const std::string& key) override;
    std::size_t invalidatePrefix(const std::string& prefix) override;
    bool healthy() const override;

private:
    std::unique_ptr<sw::redis::Redis> redis_;
    std::atomic<bool> healthy_{false};
};

} // namespace monggle
