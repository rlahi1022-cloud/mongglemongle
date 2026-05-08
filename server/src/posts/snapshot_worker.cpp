#include "monggle/posts/snapshot_worker.h"
#include "monggle/metrics/metrics.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>

#include <sstream>

namespace monggle {

namespace {

drogon::orm::DbClientPtr db() {
    return drogon::app().getDbClient("monggle_db");
}

std::string nowMinusHoursIso(int hours) {
    auto t = std::time(nullptr) - hours * 3600;
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf) + ".000";
}

} // namespace

SnapshotWorker::SnapshotWorker() : opts_(Options{}) {}
SnapshotWorker::SnapshotWorker(Options opts) : opts_(opts) {}

SnapshotWorker::~SnapshotWorker() {
    stop();
}

void SnapshotWorker::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread([this]() { loop(); });
}

void SnapshotWorker::stop() {
    if (!running_.exchange(false)) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        cv_.notify_all();
    }
    if (thread_.joinable()) thread_.join();
}

void SnapshotWorker::loop() {
    while (running_.load()) {
        try {
            int n = takeSnapshotsForActiveUsers();
            cyclesRun_.fetch_add(1);
            snapshotsTotal_.fetch_add(n);
            Metrics::instance().setGauge("snapshot_worker_last_cycle_users", {}, static_cast<double>(n));
        } catch (const std::exception& e) {
            LOG_WARN << "[snapshot-worker] cycle error: " << e.what();
        }
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait_for(lk, opts_.interval, [this]{ return !running_.load(); });
    }
}

int SnapshotWorker::runOnce() {
    return takeSnapshotsForActiveUsers();
}

int SnapshotWorker::takeSnapshotsForActiveUsers() {
    auto since = nowMinusHoursIso(static_cast<int>(opts_.lookback.count()));
    auto rows = db()->execSqlSync(
        "SELECT DISTINCT user_id FROM post_events "
        "WHERE occurred_at >= ? "
        "ORDER BY user_id "
        "LIMIT ?",
        since, static_cast<std::int64_t>(opts_.maxUsersPerCycle));

    int n = 0;
    for (const auto& row : rows) {
        auto userId = row["user_id"].as<std::int64_t>();
        if (snapshotOne(userId)) ++n;
    }
    return n;
}

bool SnapshotWorker::snapshotOne(std::int64_t userId) {
    try {
        // 1) 사용자의 현재 글 상태를 직렬화 (deleted 포함 — 시점 복원 시 needed)
        auto posts = db()->execSqlSync(
            "SELECT p.id, p.body, p.visibility, p.deleted_at, "
            "       (SELECT MAX(pe.id) FROM post_events pe WHERE pe.post_id = p.id) AS last_event_id "
            "FROM posts p "
            "WHERE p.user_id = ?",
            userId);

        Json::Value state(Json::objectValue);
        for (const auto& r : posts) {
            auto pid = r["id"].as<std::int64_t>();
            Json::Value v(Json::objectValue);
            v["body"]          = r["body"].isNull() ? "" : r["body"].as<std::string>();
            v["visibility"]    = r["visibility"].isNull() ? "private" : r["visibility"].as<std::string>();
            v["deleted"]       = !r["deleted_at"].isNull();
            v["last_event_id"] = static_cast<Json::Int64>(
                r["last_event_id"].isNull() ? 0 : r["last_event_id"].as<std::int64_t>());
            state[std::to_string(pid)] = v;
        }

        // event_cursor: 이 사용자의 post_events 중 가장 큰 id (현재 시점 기준)
        auto cur = db()->execSqlSync(
            "SELECT COALESCE(MAX(id), 0) AS m FROM post_events WHERE user_id = ?", userId);
        std::int64_t cursor = cur[0]["m"].as<std::int64_t>();

        Json::StreamWriterBuilder b;
        b["indentation"] = "";
        std::string serialized = Json::writeString(b, state);

        // 2) snapshots 적재 — taken_at = NOW, state_json = serialized
        db()->execSqlSync(
            "INSERT INTO snapshots (user_id, taken_at, state_json, event_cursor) "
            "VALUES (?, NOW(3), ?, ?)",
            userId, serialized, cursor);

        return true;
    } catch (const std::exception& e) {
        LOG_WARN << "[snapshot-worker] user " << userId << " failed: " << e.what();
        return false;
    }
}

} // namespace monggle
