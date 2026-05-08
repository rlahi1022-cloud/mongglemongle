#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/notifications/notifications_service.h"
#include "monggle/notifications/stream_hub.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <chrono>
#include <cstring>
#include <memory>

namespace monggle {

namespace {

drogon::HttpResponsePtr problemJson(drogon::HttpStatusCode status,
                                    const std::string& title,
                                    const std::string& detail,
                                    const std::string& instance) {
    Json::Value body;
    body["type"]     = "https://monggle.local/errors/" + title;
    body["title"]    = title;
    body["status"]   = static_cast<int>(status);
    body["detail"]   = detail;
    body["instance"] = instance;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(status);
    return resp;
}

Json::Value notifToJson(const NotificationItem& n) {
    Json::Value j(Json::objectValue);
    j["id"]         = static_cast<Json::Int64>(n.id);
    j["kind"]       = n.kind;
    j["actor_id"]   = n.actorId   ? Json::Value(static_cast<Json::Int64>(*n.actorId))   : Json::Value(Json::nullValue);
    j["target_id"]  = n.targetId  ? Json::Value(static_cast<Json::Int64>(*n.targetId))  : Json::Value(Json::nullValue);
    j["body"]       = n.body;
    j["is_read"]    = n.isRead;
    j["created_at"] = n.createdAt;
    j["actor_name"] = n.actorName;
    return j;
}

} // namespace

void configureNotificationsRoutes(std::shared_ptr<AuthService> authService,
                                  std::shared_ptr<NotificationsService> notif) {
    // GET /me/notifications
    drogon::app().registerHandler(
        "/me/notifications",
        [authService, notif](const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/notifications"));
                return;
            }
            auto items = notif->recent(*userId, 30);
            int unread = notif->unreadCount(*userId);
            Json::Value body(Json::objectValue);
            Json::Value arr(Json::arrayValue);
            for (const auto& n : items) arr.append(notifToJson(n));
            body["items"]  = arr;
            body["unread"] = unread;
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});

    // POST /me/notifications/read — 모두 읽음 처리
    drogon::app().registerHandler(
        "/me/notifications/read",
        [authService, notif](const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required", "/me/notifications/read"));
                return;
            }
            notif->markAllRead(*userId);
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        {drogon::Post});

    // GET /me/notifications/stream — Server-Sent Events.
    // 인증된 사용자별 스트림 hub에 등록 후 새 알림을 즉시 forward.
    // 클라이언트(EventSource)는 끊기면 자동 재연결.
    drogon::app().registerHandler(
        "/me/notifications/stream",
        [authService](const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // SSE 인증: Authorization 헤더 + (브라우저 EventSource는 헤더 못 세팅하므로) ?token= 도 허용
            std::string bearer = std::string(req->getHeader("Authorization"));
            if (bearer.empty()) {
                auto t = req->getParameter("token");
                if (!t.empty()) bearer = "Bearer " + t;
            }
            auto userId = authService->verifyAccess(bearer);
            if (!userId) {
                cb(problemJson(drogon::k401Unauthorized, "unauthorized",
                               "valid Bearer access token required (header or ?token=)",
                               "/me/notifications/stream"));
                return;
            }

            auto stream = NotificationStreamHub::instance().registerStream(*userId);

            // 스트림 응답: callback이 0 반환 시 종료, nullptr 받으면 cleanup.
            auto resp = drogon::HttpResponse::newStreamResponse(
                [stream](char* buf, std::size_t bufSize) -> std::size_t {
                    if (!buf) {
                        std::lock_guard<std::mutex> lk(stream->mu);
                        stream->closed = true;
                        stream->cv.notify_all();
                        return 0;
                    }
                    std::unique_lock<std::mutex> lk(stream->mu);
                    // 최대 15초 keep-alive 간격
                    stream->cv.wait_for(lk, std::chrono::seconds(15), [&] {
                        return !stream->queue.empty() || stream->closed;
                    });
                    if (stream->closed) return 0;
                    if (stream->queue.empty()) {
                        // SSE 주석 라인 — 클라이언트는 무시, 프록시 idle timeout 방지
                        const char ka[] = ":\n\n";
                        std::size_t n = std::min(sizeof(ka) - 1, bufSize);
                        std::memcpy(buf, ka, n);
                        return n;
                    }
                    auto& msg = stream->queue.front();
                    std::size_t n = std::min(msg.size(), bufSize);
                    std::memcpy(buf, msg.data(), n);
                    if (n == msg.size()) {
                        stream->queue.pop_front();
                    } else {
                        msg.erase(0, n);
                    }
                    return n;
                },
                "", drogon::CT_CUSTOM, "text/event-stream");
            resp->addHeader("Cache-Control", "no-cache");
            resp->addHeader("Connection",    "keep-alive");
            resp->addHeader("X-Accel-Buffering", "no");
            cb(resp);
        },
        {drogon::Get});
}

} // namespace monggle
