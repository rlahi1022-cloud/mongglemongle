#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monggle {

// 사용자별 활성 SSE 스트림을 추적하고, NotificationsService가 알림을 만들 때
// 해당 사용자의 모든 활성 스트림으로 forward 한다.
//
// 스트림 lifetime은 weak_ptr 기반 — Drogon이 callback에 nullptr을 넘기면
// stream->closed 가 true로 토글되고, 다음 publish 순회에서 자동 정리.
class NotificationStreamHub {
public:
    struct Stream {
        std::int64_t            userId{0};
        std::mutex              mu;
        std::condition_variable cv;
        std::deque<std::string> queue;     // SSE-formatted "data: {...}\n\n" 페이로드
        bool                    closed{false};
    };

    static NotificationStreamHub& instance();

    // 새 SSE 응답을 위해 stream 등록. 호출자가 shared_ptr을 보유해야 활성 유지.
    std::shared_ptr<Stream> registerStream(std::int64_t userId);

    // 알림을 사용자의 모든 활성 스트림에 전달. 정리는 게으르게(lazy).
    void publish(std::int64_t userId, const std::string& jsonPayload);

private:
    NotificationStreamHub() = default;

    std::mutex                                                                       mu_;
    std::unordered_map<std::int64_t, std::vector<std::weak_ptr<Stream>>>             byUser_;
};

} // namespace monggle
