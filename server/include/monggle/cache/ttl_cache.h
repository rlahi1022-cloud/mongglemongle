#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace monggle {

// 단순 TTL 인메모리 캐시. L1으로 사용. 다중 프로세스 도달 시 Redis로 교체.
class TtlCache {
public:
    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, std::string value, std::chrono::seconds ttl);
    void invalidate(const std::string& key);
    // 접두사로 일괄 무효화 (e.g. "feed:7:" → 사용자 7의 모든 피드 캐시)
    std::size_t invalidatePrefix(const std::string& prefix);

    // 디버그/모니터링
    std::size_t size() const;

    // 전역 피드 캐시 싱글톤
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
