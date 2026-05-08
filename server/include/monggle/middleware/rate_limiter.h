#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace sw { namespace redis { class Redis; } }

namespace monggle {

// Rate limiter with two backends:
//   - Redis (fixed window counter via INCR + EXPIRE) — survives restart, shared across instances
//   - in-memory sliding window fallback when Redis is down or not wired
// 기획 12.8 정책을 register, setRedis가 호출되지 않은 환경(테스트)에서도 동작.
class RateLimiter {
public:
    struct Rule {
        int                       maxRequests;
        std::chrono::seconds      window;
    };

    static RateLimiter& instance();

    // group: "auth_login" | "post_create" | "search" 등
    // key  : group 안에서 분기 키 (사용자 id 또는 IP)
    bool tryAcquire(const std::string& group, const std::string& key);

    // 그룹별 정책 등록
    void setRule(const std::string& group, Rule r);

    // 잔여 시간(초) — 0 이면 즉시 가능. 응답 헤더(Retry-After)용
    int retryAfterSeconds(const std::string& group, const std::string& key);

    // Redis 백엔드 활성화. 실패 시 자동으로 in-memory로 폴백.
    void setRedis(std::shared_ptr<sw::redis::Redis> redis);
    bool redisHealthy() const { return redisHealthy_.load(); }

private:
    RateLimiter() = default;

    bool tryAcquireRedis(const std::string& group, const Rule& rule, const std::string& key);
    bool tryAcquireMemory(const std::string& group, const Rule& rule, const std::string& key);
    int  retryAfterRedis(const std::string& group, const std::string& key);
    int  retryAfterMemory(const std::string& group, const Rule& rule, const std::string& key);

    std::mutex                                              mu_;
    std::unordered_map<std::string, Rule>                   rules_;
    std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> windows_;

    std::shared_ptr<sw::redis::Redis> redis_;
    std::atomic<bool>                 redisHealthy_{false};
};

} // namespace monggle
