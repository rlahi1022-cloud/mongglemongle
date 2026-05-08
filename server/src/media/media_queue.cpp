#include "monggle/media/media_queue.h"
#include "monggle/metrics/metrics.h"

#include <sw/redis++/redis++.h>
#include <trantor/utils/Logger.h>

#include <chrono>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>

namespace monggle {

MediaQueue::MediaQueue() = default;
MediaQueue::~MediaQueue() { stopWorker(); }

void MediaQueue::setRedis(std::shared_ptr<sw::redis::Redis> redis,
                          const std::string& streamKey,
                          const std::string& groupName,
                          const std::string& consumerId) {
    redis_      = std::move(redis);
    streamKey_  = streamKey;
    groupName_  = groupName;
    consumerId_ = consumerId;
    if (!redis_) {
        healthy_.store(false);
        return;
    }
    try {
        redis_->ping();
        ensureGroup();
        healthy_.store(true);
    } catch (const std::exception& e) {
        std::cerr << "[media-queue] init failed: " << e.what() << std::endl;
        healthy_.store(false);
    }
}

void MediaQueue::ensureGroup() {
    // 그룹이 이미 있으면 BUSYGROUP 에러가 throw되니 catch.
    try {
        redis_->xgroup_create(streamKey_, groupName_, "$", true);
    } catch (const sw::redis::Error& e) {
        // BUSYGROUP은 정상 — 무시.
    }
}

bool MediaQueue::enqueue(std::int64_t mediaId, std::int64_t userId, const std::string& kind) {
    if (!healthy_.load() || !redis_) return false;
    try {
        std::vector<std::pair<std::string, std::string>> fields = {
            {"media_id", std::to_string(mediaId)},
            {"user_id",  std::to_string(userId)},
            {"kind",     kind},
        };
        redis_->xadd(streamKey_, "*", fields.begin(), fields.end());
        Metrics::instance().incCounter("media_queue_enqueued_total", {{"kind", kind}});
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[media-queue] enqueue failed: " << e.what() << std::endl;
        healthy_.store(false);
        return false;
    }
}

void MediaQueue::startWorker(Handler handler) {
    if (running_.exchange(true)) return;
    handler_ = std::move(handler);
    worker_  = std::thread([this]() { workerLoop(); });
}

void MediaQueue::stopWorker() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

void MediaQueue::workerLoop() {
    using StreamItem = std::pair<std::string, std::vector<std::pair<std::string, std::string>>>;
    using StreamResult = std::unordered_map<std::string, std::vector<StreamItem>>;

    while (running_.load()) {
        if (!healthy_.load() || !redis_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        try {
            StreamResult result;
            // ">" = 아직 다른 컨슈머에 전달되지 않은 새 메시지만
            redis_->xreadgroup(groupName_, consumerId_, streamKey_, ">", 16,
                               std::inserter(result, result.end()));

            if (result.empty()) {
                // 새 메시지 없을 때 짧게 대기 (블로킹 대신 polling으로 단순화)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            for (const auto& [stream, entries] : result) {
                for (const auto& [entryId, fields] : entries) {
                    Job job;
                    job.redisId = entryId;
                    for (const auto& [k, v] : fields) {
                        if      (k == "media_id") job.mediaId = std::stoll(v);
                        else if (k == "user_id")  job.userId  = std::stoll(v);
                        else if (k == "kind")     job.kind    = v;
                    }
                    bool ok = false;
                    try {
                        ok = handler_ ? handler_(job) : true;
                    } catch (const std::exception& e) {
                        LOG_WARN << "[media-queue] handler exception: " << e.what();
                    }
                    if (ok) {
                        try {
                            redis_->xack(streamKey_, groupName_, entryId);
                            processed_.fetch_add(1);
                            Metrics::instance().incCounter(
                                "media_queue_processed_total", {{"kind", job.kind}});
                        } catch (const std::exception& e) {
                            std::cerr << "[media-queue] xack failed: " << e.what() << std::endl;
                        }
                    } else {
                        Metrics::instance().incCounter(
                            "media_queue_retried_total", {{"kind", job.kind}});
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[media-queue] worker loop error: " << e.what() << std::endl;
            healthy_.store(false);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            healthy_.store(true); // 다음 사이클에 다시 시도
        }
    }
}

} // namespace monggle
