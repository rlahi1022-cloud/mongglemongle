#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace sw { namespace redis { class Redis; } }

namespace monggle {

// Redis Stream 기반 미디어 처리 큐.
//
// Stream key:        media:jobs
// Consumer group:    media-workers
// Job payload (JSON): { "media_id": <int64>, "kind": "photo"|"video", "user_id": <int64> }
//
// enqueue()는 XADD, consumer는 XREADGROUP + XACK.
// 워커가 다운돼도 unacked 메시지는 다른 워커가 다시 처리(at-least-once).
class MediaQueue {
public:
    struct Job {
        std::string  redisId;     // Stream entry ID (XACK용)
        std::int64_t mediaId{0};
        std::int64_t userId{0};
        std::string  kind;        // "photo" | "video"
    };

    using Handler = std::function<bool(const Job&)>;

    MediaQueue();
    ~MediaQueue();

    void setRedis(std::shared_ptr<sw::redis::Redis> redis,
                  const std::string& streamKey  = "media:jobs",
                  const std::string& groupName  = "media-workers",
                  const std::string& consumerId = "monggle-c1");

    bool enqueue(std::int64_t mediaId, std::int64_t userId, const std::string& kind);

    // 워커 thread 시작/종료. handler가 true면 ack, false면 retain (다음 사이클에 재처리).
    void startWorker(Handler handler);
    void stopWorker();

    bool healthy() const { return healthy_.load(); }
    std::int64_t processedTotal() const { return processed_.load(); }

private:
    void ensureGroup();
    void workerLoop();

    std::shared_ptr<sw::redis::Redis> redis_;
    std::string                       streamKey_;
    std::string                       groupName_;
    std::string                       consumerId_;
    std::atomic<bool>                 healthy_{false};

    std::thread                       worker_;
    std::atomic<bool>                 running_{false};
    Handler                           handler_;

    std::atomic<std::int64_t>         processed_{0};
};

} // namespace monggle
