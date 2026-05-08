#include "monggle/middleware/rate_limiter.h"
#include "monggle/metrics/metrics.h"

#include <sw/redis++/redis++.h>

#include <algorithm>
#include <iostream>

namespace monggle {

RateLimiter& RateLimiter::instance() {
    static RateLimiter inst;
    static bool initialized = false;
    if (!initialized) {
        // 기획 12.8 — Rate Limiting 기본 정책
        inst.setRule("auth_login",   { 5,   std::chrono::seconds(60)        });
        inst.setRule("post_create",  { 10,  std::chrono::seconds(60)        });
        inst.setRule("search",       { 30,  std::chrono::seconds(60)        });
        inst.setRule("media_upload", { 100, std::chrono::seconds(60 * 60)   });
        initialized = true;
    }
    return inst;
}

void RateLimiter::setRule(const std::string& group, Rule r) {
    std::lock_guard<std::mutex> lk(mu_);
    rules_[group] = r;
}

void RateLimiter::setRedis(std::shared_ptr<sw::redis::Redis> redis) {
    if (!redis) {
        redis_.reset();
        redisHealthy_.store(false);
        return;
    }
    try {
        redis->ping();
        std::lock_guard<std::mutex> lk(mu_);
        redis_ = std::move(redis);
        redisHealthy_.store(true);
    } catch (const std::exception& e) {
        std::cerr << "[ratelimit] redis ping failed: " << e.what()
                  << " — staying on in-memory backend" << std::endl;
        redisHealthy_.store(false);
    }
}

bool RateLimiter::tryAcquire(const std::string& group, const std::string& key) {
    Rule rule;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto rIt = rules_.find(group);
        if (rIt == rules_.end()) return true; // 정책 없음 = 무제한
        rule = rIt->second;
    }

    bool ok = (redisHealthy_.load() && redis_)
              ? tryAcquireRedis(group, rule, key)
              : tryAcquireMemory(group, rule, key);
    if (!ok) {
        Metrics::instance().incCounter("ratelimit_denied_total", {{"group", group}});
    }
    return ok;
}

int RateLimiter::retryAfterSeconds(const std::string& group, const std::string& key) {
    Rule rule;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto rIt = rules_.find(group);
        if (rIt == rules_.end()) return 0;
        rule = rIt->second;
    }
    if (redisHealthy_.load() && redis_) {
        return retryAfterRedis(group, key);
    }
    return retryAfterMemory(group, rule, key);
}

bool RateLimiter::tryAcquireRedis(const std::string& group, const Rule& rule, const std::string& key) {
    // Fixed-window counter: INCR + EXPIRE on first hit. Atomicity 부족(첫 set과 expire 사이의
    // race)은 윈도우 경계에서 미세하게 발생할 수 있으나, MULTI/EXEC pipeline으로 방어.
    const std::string fullKey = "rl:" + group + ":" + key;
    try {
        auto pipe = redis_->pipeline(false);
        pipe.incr(fullKey);
        pipe.expire(fullKey, rule.window);
        auto replies = pipe.exec();
        long long count = replies.get<long long>(0);
        return count <= rule.maxRequests;
    } catch (const std::exception& e) {
        std::cerr << "[ratelimit] redis error: " << e.what()
                  << " — falling back to in-memory" << std::endl;
        redisHealthy_.store(false);
        return tryAcquireMemory(group, rule, key);
    }
}

int RateLimiter::retryAfterRedis(const std::string& group, const std::string& key) {
    const std::string fullKey = "rl:" + group + ":" + key;
    try {
        auto ttl = redis_->ttl(fullKey);
        if (ttl < 0) return 0;
        return static_cast<int>(ttl);
    } catch (const std::exception& e) {
        std::cerr << "[ratelimit] redis ttl error: " << e.what() << std::endl;
        redisHealthy_.store(false);
        return 0;
    }
}

bool RateLimiter::tryAcquireMemory(const std::string& group, const Rule& rule, const std::string& key) {
    std::lock_guard<std::mutex> lk(mu_);
    auto now = std::chrono::steady_clock::now();
    auto& q  = windows_[group + ":" + key];

    while (!q.empty() && (now - q.front()) > rule.window) q.pop_front();

    if (static_cast<int>(q.size()) >= rule.maxRequests) return false;
    q.push_back(now);
    return true;
}

int RateLimiter::retryAfterMemory(const std::string& group, const Rule& rule, const std::string& key) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = windows_.find(group + ":" + key);
    if (it == windows_.end() || it->second.empty()) return 0;

    auto now = std::chrono::steady_clock::now();
    auto wait = rule.window - std::chrono::duration_cast<std::chrono::seconds>(now - it->second.front());
    return wait.count() > 0 ? static_cast<int>(wait.count()) : 0;
}

} // namespace monggle
