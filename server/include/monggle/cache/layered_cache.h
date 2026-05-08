#pragma once

#include "monggle/cache/icache.h"
#include "monggle/cache/redis_cache.h"
#include "monggle/cache/ttl_cache.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace monggle {

// 2계층 캐시.
// 읽기: L1 hit → 반환. L1 miss + L2 hit → L1에 짧은 TTL로 채우고 반환. 둘 다 miss → nullopt.
// 쓰기: L1과 L2에 동일 TTL로 set.
// 무효화: L1과 L2 둘 다 erase. prefix는 둘 다 scan.
//
// 의도적 비일관성: L1 TTL은 짧고(<= L2 TTL), 다중 인스턴스 환경에서도 30초 이내에는 자연 수렴.
// 정합성이 critical하지 않은 캐시 데이터에만 사용.
class LayeredCache : public ICache {
public:
    // l2가 nullptr이면 L1 only로 동작 (Redis 미가용 환경 호환).
    LayeredCache(std::shared_ptr<TtlCache> l1, std::shared_ptr<RedisCache> l2);

    std::optional<std::string> get(const std::string& key) override;
    void set(const std::string& key, std::string value, std::chrono::seconds ttl) override;
    void invalidate(const std::string& key) override;
    std::size_t invalidatePrefix(const std::string& prefix) override;

    bool healthy() const override; // L1만으로도 healthy=true. L2 상태는 별도 확인.
    bool l2Healthy() const;        // L2 단독 상태 — readyz 등에서 사용.

    // 전역 피드 캐시 인스턴스 (init()에서 wiring).
    static LayeredCache& feed();
    static void initFeed(std::shared_ptr<TtlCache> l1, std::shared_ptr<RedisCache> l2);

private:
    std::shared_ptr<TtlCache> l1_;
    std::shared_ptr<RedisCache> l2_;
    std::chrono::seconds l1Cap_{30}; // L1은 항상 짧은 TTL로 캡 (사용자 ttl과 min)
};

} // namespace monggle
