#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monggle {

// 인프로세스 pub/sub. 동기 디스패치 (subscriber가 빠르다고 가정).
// 워터마크: published 누적 수 노출. 진짜 백프레셔는 Redis Stream 도입 후.
class EventBus {
public:
    using Handler = std::function<void(const std::string& payload)>;

    void subscribe(const std::string& topic, Handler h);

    // 동기적으로 모든 subscriber 호출. 예외는 catch해서 무시 (한 subscriber 실패가
    // 다른 subscriber를 막지 않게).
    void publish(const std::string& topic, const std::string& payload);

    std::size_t published() const { return published_.load(); }

private:
    mutable std::mutex mu_;
    std::unordered_map<std::string, std::vector<Handler>> subs_;
    std::atomic<std::size_t> published_{0};
};

// 토픽 상수 (typo 방지)
namespace topics {
constexpr const char* kPostCreated   = "post.created";
constexpr const char* kPostEdited    = "post.edited";
constexpr const char* kPostDeleted   = "post.deleted";
constexpr const char* kMediaUploaded = "media.uploaded";
constexpr const char* kFollowAdded   = "follow.added";
constexpr const char* kCommentAdded  = "comment.added";
} // namespace topics

} // namespace monggle
