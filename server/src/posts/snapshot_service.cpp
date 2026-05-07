#include "monggle/posts/snapshot_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <json/json.h>
#include <json/reader.h>

#include <map>
#include <sstream>

namespace monggle {

namespace {

drogon::orm::DbClientPtr db() {
    return drogon::app().getDbClient("monggle_db");
}

// "YYYY-MM-DDTHH:MM:SS" → "YYYY-MM-DD HH:MM:SS" (MariaDB DATETIME 호환)
std::string normalizeIso(const std::string& s) {
    std::string out = s;
    auto t = out.find('T');
    if (t != std::string::npos) out[t] = ' ';
    // 끝의 'Z' 제거
    if (!out.empty() && out.back() == 'Z') out.pop_back();
    return out;
}

Json::Value parseJson(const std::string& raw) {
    Json::Value v;
    Json::CharReaderBuilder b;
    std::string err;
    auto reader = std::unique_ptr<Json::CharReader>(b.newCharReader());
    reader->parse(raw.data(), raw.data() + raw.size(), &v, &err);
    return v;
}

void applyEvent(std::map<std::int64_t, PostStateAt>& state,
                std::int64_t eventId,
                std::int64_t postId,
                const std::string& eventType,
                const std::string& payloadJson) {
    auto payload = parseJson(payloadJson);

    auto it = state.find(postId);

    if (eventType == "created") {
        PostStateAt p{};
        p.id          = postId;
        p.body        = payload.get("body", "").asString();
        p.visibility  = payload.get("visibility", "private").asString();
        p.deleted     = false;
        p.lastEventId = eventId;
        state[postId] = p;
        return;
    }

    if (it == state.end()) {
        // 이벤트 순서가 깨졌거나 (created 없이 edited 등) — payload만으로 가능한 만큼 채움
        PostStateAt p{};
        p.id          = postId;
        p.body        = payload.get("body", "").asString();
        p.visibility  = "private";
        p.deleted     = (eventType == "deleted");
        p.lastEventId = eventId;
        state[postId] = p;
        return;
    }

    if (eventType == "edited") {
        if (payload.isMember("body")) it->second.body = payload["body"].asString();
        it->second.lastEventId = eventId;
    } else if (eventType == "visibility_changed") {
        if (payload.isMember("to")) it->second.visibility = payload["to"].asString();
        it->second.lastEventId = eventId;
    } else if (eventType == "deleted") {
        it->second.deleted     = true;
        it->second.lastEventId = eventId;
    } else if (eventType == "media_added") {
        // C5에서 다룸. MVP 시점 복원에는 미반영
        it->second.lastEventId = eventId;
    }
}

} // namespace

SnapResult<UserStateAt> SnapshotService::restoreState(std::int64_t userId,
                                                       const std::string& targetTimeIso) {
    auto target = normalizeIso(targetTimeIso);
    if (target.empty()) {
        return SnapshotError{SnapshotError::BadRequest, "at parameter required"};
    }

    try {
        std::map<std::int64_t, PostStateAt> state;
        std::int64_t cursor = 0;

        // 1) 스냅샷 로드 (없으면 처음부터)
        auto snaps = db()->execSqlSync(
            "SELECT id, event_cursor, state_json FROM snapshots "
            "WHERE user_id = ? AND taken_at <= ? "
            "ORDER BY taken_at DESC LIMIT 1",
            userId, target);

        if (snaps.size() > 0) {
            cursor = snaps[0]["event_cursor"].as<std::int64_t>();
            // 저장 형식: gzip 압축된 JSON. MVP에서는 비압축 JSON 가정.
            // 실제 워커가 만들기 전이라 일단 디코드 시도, 실패하면 처음부터 재생.
            try {
                auto raw   = snaps[0]["state_json"].as<std::string>();
                auto state_j = parseJson(raw);
                if (state_j.isObject()) {
                    for (const auto& key : state_j.getMemberNames()) {
                        const auto& v = state_j[key];
                        PostStateAt p{};
                        p.id          = std::stoll(key);
                        p.body        = v.get("body", "").asString();
                        p.visibility  = v.get("visibility", "private").asString();
                        p.deleted     = v.get("deleted", false).asBool();
                        p.lastEventId = v.get("last_event_id", 0).asInt64();
                        state[p.id]   = p;
                    }
                }
            } catch (...) {
                // 비정상 스냅샷 — 처음부터 재생
                state.clear();
                cursor = 0;
            }
        }

        // 2) cursor 이후 ~ target 까지 이벤트 재생
        auto events = db()->execSqlSync(
            "SELECT id, post_id, event_type, payload_json "
            "FROM post_events "
            "WHERE user_id = ? AND id > ? AND occurred_at <= ? "
            "ORDER BY id ASC",
            userId, cursor, target);

        for (const auto& row : events) {
            applyEvent(state,
                       row["id"].as<std::int64_t>(),
                       row["post_id"].as<std::int64_t>(),
                       row["event_type"].as<std::string>(),
                       row["payload_json"].as<std::string>());
        }

        UserStateAt out;
        out.userId     = userId;
        out.targetTime = target;
        out.posts.reserve(state.size());
        for (auto& [_, p] : state) {
            out.posts.push_back(p);
        }
        return out;
    } catch (const drogon::orm::DrogonDbException& e) {
        return SnapshotError{SnapshotError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return SnapshotError{SnapshotError::InternalError, e.what()};
    }
}

} // namespace monggle
