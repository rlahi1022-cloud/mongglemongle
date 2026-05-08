#pragma once

#include "monggle/cache/icache.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace monggle {

// 단순 TTL 인메모리 캐시. L1으로 사용. 다중 프로세스 환경에서는 L2(Redis)와 함께 사용.
class TtlCache : public ICache {
public:
    std::optional<std::string> get(const std::string& key) override;
    void set(const std::string& key, std::string value, std::chrono::seconds ttl) override;
    void invalidate(const std::string& key) override;
    // 접두사로 일괄 무효화 (e.g. "feed:7:" → 사용자 7의 모든 피드 캐시)
    std::size_t invalidatePrefix(const std::string& prefix) override;

    // 디버그/모니터링
    std::size_t size() const;

    // 전역 피드 L1 캐시 싱글톤 (legacy 호환). 신규 코드는 LayeredCache::feed()를 사용.
    static TtlCache& feedCache();

private:
    struct Entry {
        std::string value;
        std::chrono::steady_clock::time_point expiresAt;
    };
    mutable std::mutex mu_;
    std::unordered_map<std::string, Entry> map_;
};

} // namespace monggle
