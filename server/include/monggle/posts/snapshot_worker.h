#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace monggle {

// 주기적 사용자 스냅샷 워커.
//
// 시동 시 std::thread 한 개를 띄우고, intervalSeconds 마다:
//   1) 최근 lookbackHours 이내에 이벤트가 있던 사용자 ID 목록을 가져와
//   2) 각 사용자의 현재 글 상태를 JSON으로 직렬화해 snapshots 테이블에 INSERT
//   3) event_cursor는 그 시점까지 반영한 post_events.id 의 max
//
// 시점 복원(SnapshotService::restoreState)은 가장 최근 (taken_at <= target) 스냅샷을
// 로드한 뒤 그 이후 이벤트만 재생하므로, 워커가 도는 만큼 평균 재생 비용이 줄어든다.
//
// 종료 시 stop()을 호출하면 cv가 깨우고 워커가 정상 종료한다.
class SnapshotWorker {
public:
    struct Options {
        std::chrono::seconds interval{std::chrono::hours(1)};   // 스냅샷 주기
        std::chrono::hours   lookback{24};                      // 활성 사용자 조회 윈도우
        int                  maxUsersPerCycle{500};             // 한 사이클 처리량 상한
    };

    SnapshotWorker();
    explicit SnapshotWorker(Options opts);
    ~SnapshotWorker();

    void start();
    void stop();

    // 디버그/테스트 — 즉시 한 사이클 실행. 성공 시 처리한 사용자 수 반환.
    int runOnce();

    // 통계
    std::int64_t cyclesRun()      const { return cyclesRun_.load(); }
    std::int64_t snapshotsTotal() const { return snapshotsTotal_.load(); }

private:
    void loop();
    int  takeSnapshotsForActiveUsers();
    bool snapshotOne(std::int64_t userId);

    Options                  opts_;
    std::thread              thread_;
    std::mutex               mu_;
    std::condition_variable  cv_;
    std::atomic<bool>        running_{false};

    std::atomic<std::int64_t> cyclesRun_{0};
    std::atomic<std::int64_t> snapshotsTotal_{0};
};

} // namespace monggle
