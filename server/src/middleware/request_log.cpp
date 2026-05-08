#include "monggle/middleware/request_log.h"
#include "monggle/metrics/metrics.h"

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>

namespace monggle {

namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(c);
        }
    }
    return out;
}

// 라벨 카디널리티 폭증을 막기 위해 path를 정규화.
// 숫자 ID 세그먼트는 ":id"로 치환. /me/feed → /me/feed, /posts/42/comments → /posts/:id/comments
std::string normalizePath(const std::string& path) {
    std::string out;
    out.reserve(path.size());
    std::size_t i = 0;
    while (i < path.size()) {
        if (path[i] == '/') {
            out.push_back('/');
            std::size_t j = i + 1;
            while (j < path.size() && path[j] != '/') ++j;
            std::string seg = path.substr(i + 1, j - i - 1);
            bool allDigits = !seg.empty() &&
                std::all_of(seg.begin(), seg.end(), [](char c) {
                    return c >= '0' && c <= '9';
                });
            if (allDigits) out += ":id";
            else out += seg;
            i = j;
        } else {
            out.push_back(path[i]);
            ++i;
        }
    }
    return out;
}

std::int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

void installRequestLog() {
    // 요청 시작 시각을 X-Internal-Start 어트리뷰트에 박아두는 advice
    drogon::app().registerPreHandlingAdvice(
        [](const drogon::HttpRequestPtr& req) {
            req->attributes()->insert("monggle.start_ns", nowNanos());
        });

    drogon::app().registerPreSendingAdvice(
        [](const drogon::HttpRequestPtr& req,
           const drogon::HttpResponsePtr& resp) {
            const auto& q = req->getQuery();
            const auto& method = req->getMethodString();
            const auto& path   = req->getPath();
            const int   status = static_cast<int>(resp->getStatusCode());
            const auto& peer   = req->getPeerAddr().toIp();

            std::int64_t elapsedNs = 0;
            if (req->attributes()->find("monggle.start_ns")) {
                auto start = req->attributes()->get<std::int64_t>("monggle.start_ns");
                elapsedNs = nowNanos() - start;
            }
            double elapsedSec = static_cast<double>(elapsedNs) / 1e9;

            // JSON 한 줄 — Loki/CloudWatch/ELK 어디든 그대로 흡수 가능.
            std::ostringstream js;
            js << "{\"ts\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch()).count()
               << ",\"method\":\"" << method << "\""
               << ",\"path\":\""   << jsonEscape(path) << "\"";
            if (!q.empty()) js << ",\"query\":\""  << jsonEscape(q) << "\"";
            js << ",\"status\":"   << status
               << ",\"peer\":\""   << jsonEscape(peer) << "\""
               << ",\"elapsed_ms\":" << static_cast<int>(elapsedSec * 1000.0)
               << "}";
            LOG_INFO << js.str();

            // Prometheus metrics
            auto& m = Metrics::instance();
            std::string normPath = normalizePath(path);
            m.incCounter("http_requests_total",
                         {{"method", method}, {"path", normPath},
                          {"status", std::to_string(status)}});
            m.observeHistogram("http_request_duration_seconds",
                               {{"method", method}, {"path", normPath}},
                               elapsedSec);
        });
}

} // namespace monggle
