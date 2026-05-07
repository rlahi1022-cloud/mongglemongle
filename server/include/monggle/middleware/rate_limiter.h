#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace monggle {

// 단순 슬라이딩 윈도우 카운터 (메모리 기반).
// 여러 프로세스/머신으로 확장 시 Redis sorted-set 또는 token bucket 으로 교체.
class RateLimiter {
public:
    struct Rule {
        int                       maxRequests;
        std::chrono::seconds      window;
    };

    // 기획 12.8 기본 정책
    static RateLimiter& instance();

    // group: "auth_login" | "post_create" | "search" 등
    // key  : group 안에서 분기 키 (사용자 id 또는 IP)
    bool tryAcquire(const std::string& group, const std::string& key);

    // 그룹별 정책 등록
    void setRule(const std::string& group, Rule r);

    // 잔여 시간(초) — 0 이면 즉시 가능. 응답 헤더(Retry-After)용
    int retryAfterSeconds(const std::string& group, const std::string& key);

private:
    RateLimiter() = default;

    std::mutex                                              mu_;
    std::unordered_map<std::string, Rule>                   rules_;
    std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> windows_;
};

} // namespace monggle
