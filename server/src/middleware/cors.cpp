#include "monggle/middleware/cors.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <memory>

namespace monggle {

namespace {

bool originAllowed(const CorsConfig& cfg, const std::string& origin) {
    return std::find(cfg.allowedOrigins.begin(), cfg.allowedOrigins.end(), origin)
           != cfg.allowedOrigins.end();
}

void addCommonCorsHeaders(const CorsConfig& cfg,
                          const std::string& origin,
                          drogon::HttpResponsePtr& resp) {
    if (!origin.empty() && originAllowed(cfg, origin)) {
        resp->addHeader("Access-Control-Allow-Origin", origin);
        // Vary: Origin 으로 캐시 분리 안내
        resp->addHeader("Vary", "Origin");
        if (cfg.allowCredentials) {
            resp->addHeader("Access-Control-Allow-Credentials", "true");
        }
    }
    if (!cfg.exposedHeaders.empty()) {
        resp->addHeader("Access-Control-Expose-Headers", cfg.exposedHeaders);
    }
}

} // namespace

CorsConfig defaultDevCors() {
    CorsConfig c;
    c.allowedOrigins   = {
        "http://localhost:5173",  // Vite 기본
        "http://127.0.0.1:5173",
        "http://localhost:3000",  // Next.js / CRA 기본
    };
    c.allowedMethods   = "GET, POST, PUT, PATCH, DELETE, OPTIONS";
    c.allowedHeaders   = "Authorization, Content-Type, Idempotency-Key";
    c.exposedHeaders   = "Content-Disposition";
    c.maxAge           = "600";
    c.allowCredentials = true;
    return c;
}

void installCors(const CorsConfig& cfg) {
    auto cfgPtr = std::make_shared<CorsConfig>(cfg);

    // Preflight (OPTIONS) 가로채기 — 핸들러 무관하게 200 + CORS 헤더 반환
    drogon::app().registerSyncAdvice(
        [cfgPtr](const drogon::HttpRequestPtr& req) -> drogon::HttpResponsePtr {
            if (req->getMethod() != drogon::Options) return {};

            auto origin = std::string(req->getHeader("Origin"));
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            addCommonCorsHeaders(*cfgPtr, origin, resp);
            resp->addHeader("Access-Control-Allow-Methods", cfgPtr->allowedMethods);
            resp->addHeader("Access-Control-Allow-Headers", cfgPtr->allowedHeaders);
            resp->addHeader("Access-Control-Max-Age",       cfgPtr->maxAge);
            return resp;
        });

    // 모든 응답에 CORS 헤더 (브라우저가 받기 위함)
    drogon::app().registerPreSendingAdvice(
        [cfgPtr](const drogon::HttpRequestPtr& req,
                 const drogon::HttpResponsePtr& resp) {
            auto origin = std::string(req->getHeader("Origin"));
            // resp const& 인데 헤더 추가는 가능
            auto mut = std::const_pointer_cast<drogon::HttpResponse>(resp);
            addCommonCorsHeaders(*cfgPtr, origin, mut);
        });
}

} // namespace monggle
