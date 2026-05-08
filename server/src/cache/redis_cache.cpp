#include "monggle/cache/redis_cache.h"

#include <sw/redis++/redis++.h>

#include <iostream>
#include <iterator>
#include <vector>

namespace monggle {

namespace {

sw::redis::ConnectionOptions makeConnOpts(const RedisCache::Options& o) {
    sw::redis::ConnectionOptions co;
    co.host = o.host;
    co.port = o.port;
    co.connect_timeout = o.connectTimeout;
    co.socket_timeout = o.socketTimeout;
    return co;
}

sw::redis::ConnectionPoolOptions makePoolOpts(const RedisCache::Options& o) {
    sw::redis::ConnectionPoolOptions po;
    po.size = o.poolSize;
    po.wait_timeout = std::chrono::milliseconds(50);
    return po;
}

} // namespace

RedisCache::RedisCache(const Options& opts) {
    try {
        redis_ = std::make_unique<sw::redis::Redis>(makeConnOpts(opts), makePoolOpts(opts));
        // Eager connection check — PING surfaces a refused/timeout immediately
        // so we don't lie about being healthy on startup.
        redis_->ping();
        healthy_.store(true);
    } catch (const std::exception& e) {
        std::cerr << "[redis] connect failed: " << e.what() << " — degrading to L1-only" << std::endl;
        healthy_.store(false);
    }
}

RedisCache::~RedisCache() = default;

bool RedisCache::healthy() const {
    return healthy_.load();
}

std::optional<std::string> RedisCache::get(const std::string& key) {
    if (!healthy_.load()) return std::nullopt;
    try {
        auto v = redis_->get(key);
        if (v) return *v;
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "[redis] get error: " << e.what() << std::endl;
        healthy_.store(false);
        return std::nullopt;
    }
}

void RedisCache::set(const std::string& key, std::string value, std::chrono::seconds ttl) {
    if (!healthy_.load()) return;
    try {
        redis_->set(key, value, ttl);
    } catch (const std::exception& e) {
        std::cerr << "[redis] set error: " << e.what() << std::endl;
        healthy_.store(false);
    }
}

void RedisCache::invalidate(const std::string& key) {
    if (!healthy_.load()) return;
    try {
        redis_->del(key);
    } catch (const std::exception& e) {
        std::cerr << "[redis] del error: " << e.what() << std::endl;
        healthy_.store(false);
    }
}

std::size_t RedisCache::invalidatePrefix(const std::string& prefix) {
    if (!healthy_.load()) return 0;
    try {
        // SCAN으로 cursor-based 순회. KEYS는 운영에서 차단해야 하는 명령어.
        // 한 배치당 100개 키씩 모아 UNLINK(비동기 free)로 삭제.
        std::vector<std::string> batch;
        long long cursor = 0;
        std::size_t total = 0;
        const std::string match = prefix + "*";
        do {
            batch.clear();
            cursor = redis_->scan(cursor, match, 256, std::back_inserter(batch));
            if (!batch.empty()) {
                total += redis_->unlink(batch.begin(), batch.end());
            }
        } while (cursor != 0);
        return total;
    } catch (const std::exception& e) {
        std::cerr << "[redis] scan/unlink error: " << e.what() << std::endl;
        healthy_.store(false);
        return 0;
    }
}

} // namespace monggle
