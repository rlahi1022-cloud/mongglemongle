#include "monggle/middleware/rate_limiter.h"

#include <algorithm>

namespace monggle {

RateLimiter& RateLimiter::instance() {
    static RateLimiter inst;
    static bool initialized = false;
    if (!initialized) {
        // 기획 12.8 — Rate Limiting 기본 정책
        inst.setRule("auth_login",   { 5, std::chrono::seconds(60)        });
        inst.setRule("post_create",  { 10, std::chrono::seconds(60)       });
        inst.setRule("search",       { 30, std::chrono::seconds(60)       });
        inst.setRule("media_upload", { 100, std::chrono::seconds(60 * 60) });
        initialized = true;
    }
    return inst;
}

void RateLimiter::setRule(const std::string& group, Rule r) {
    std::lock_guard<std::mutex> lk(mu_);
    rules_[group] = r;
}

bool RateLimiter::tryAcquire(const std::string& group, const std::string& key) {
    std::lock_guard<std::mutex> lk(mu_);
    auto rIt = rules_.find(group);
    if (rIt == rules_.end()) return true;  // 정책 없음 = 무제한
    const auto& rule = rIt->second;

    auto now = std::chrono::steady_clock::now();
    auto& q  = windows_[group + ":" + key];

    while (!q.empty() && (now - q.front()) > rule.window) q.pop_front();

    if (static_cast<int>(q.size()) >= rule.maxRequests) return false;
    q.push_back(now);
    return true;
}

int RateLimiter::retryAfterSeconds(const std::string& group, const std::string& key) {
    std::lock_guard<std::mutex> lk(mu_);
    auto rIt = rules_.find(group);
    if (rIt == rules_.end()) return 0;
    const auto& rule = rIt->second;

    auto it = windows_.find(group + ":" + key);
    if (it == windows_.end() || it->second.empty()) return 0;

    auto now = std::chrono::steady_clock::now();
    auto wait = rule.window - std::chrono::duration_cast<std::chrono::seconds>(now - it->second.front());
    return wait.count() > 0 ? static_cast<int>(wait.count()) : 0;
}

} // namespace monggle
