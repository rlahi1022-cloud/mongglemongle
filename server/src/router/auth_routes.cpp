#include "monggle/router/routes.h"
#include "monggle/auth/auth_service.h"
#include "monggle/middleware/rate_limiter.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <json/json.h>

#include <memory>
#include <string>

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
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    return resp;
}

drogon::HttpResponsePtr tokenPairResponse(const TokenPair& pair, drogon::HttpStatusCode status) {
    Json::Value body;
    body["user_id"]            = static_cast<Json::Int64>(pair.userId);
    body["access_token"]       = pair.accessToken;
    body["refresh_token"]      = pair.refreshToken;
    body["access_expires_at"]  = static_cast<Json::Int64>(pair.accessExpiresAt);
    body["refresh_expires_at"] = static_cast<Json::Int64>(pair.refreshExpiresAt);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(status);
    return resp;
}

drogon::HttpResponsePtr authErrorResponse(const AuthError& err, const std::string& instance) {
    switch (err.code) {
        case AuthError::EmailTaken:
            return problemJson(drogon::k409Conflict, "email_taken", err.detail, instance);
        case AuthError::InvalidCredentials:
            return problemJson(drogon::k401Unauthorized, "invalid_credentials", err.detail, instance);
        case AuthError::InvalidRefresh:
            return problemJson(drogon::k401Unauthorized, "invalid_refresh", err.detail, instance);
        case AuthError::InternalError:
        default:
            return problemJson(drogon::k500InternalServerError, "internal_error", err.detail, instance);
    }
}

bool readJsonField(const Json::Value& body, const char* key, std::string& out) {
    if (!body.isMember(key) || !body[key].isString()) return false;
    out = body[key].asString();
    return !out.empty();
}

} // namespace

void configureAuthRoutes(std::shared_ptr<AuthService> authService) {
    drogon::app().registerHandler(
        "/auth/signup",
        [authService](const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "JSON body required", "/auth/signup"));
                return;
            }
            std::string email, password, displayName;
            if (!readJsonField(*json, "email", email) ||
                !readJsonField(*json, "password", password) ||
                !readJsonField(*json, "display_name", displayName)) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "email, password, display_name required",
                                     "/auth/signup"));
                return;
            }

            auto result = authService->signup(email, password, displayName);
            if (auto* err = std::get_if<AuthError>(&result)) {
                callback(authErrorResponse(*err, "/auth/signup"));
            } else {
                callback(tokenPairResponse(std::get<TokenPair>(result), drogon::k201Created));
            }
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/auth/login",
        [authService](const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            // Rate limit: IP 기반 분당 5회 (기획 12.8)
            std::string ip{req->getPeerAddr().toIp()};
            if (!RateLimiter::instance().tryAcquire("auth_login", ip)) {
                auto resp = problemJson(drogon::k429TooManyRequests, "rate_limited",
                                        "too many login attempts", "/auth/login");
                resp->addHeader("Retry-After", std::to_string(
                    RateLimiter::instance().retryAfterSeconds("auth_login", ip)));
                callback(resp);
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "JSON body required", "/auth/login"));
                return;
            }
            std::string email, password;
            if (!readJsonField(*json, "email", email) ||
                !readJsonField(*json, "password", password)) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "email and password required", "/auth/login"));
                return;
            }

            auto result = authService->login(email, password);
            if (auto* err = std::get_if<AuthError>(&result)) {
                callback(authErrorResponse(*err, "/auth/login"));
            } else {
                callback(tokenPairResponse(std::get<TokenPair>(result), drogon::k200OK));
            }
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/auth/refresh",
        [authService](const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "JSON body required", "/auth/refresh"));
                return;
            }
            std::string refreshToken;
            if (!readJsonField(*json, "refresh_token", refreshToken)) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "refresh_token required", "/auth/refresh"));
                return;
            }

            auto result = authService->refresh(refreshToken);
            if (auto* err = std::get_if<AuthError>(&result)) {
                callback(authErrorResponse(*err, "/auth/refresh"));
            } else {
                callback(tokenPairResponse(std::get<TokenPair>(result), drogon::k200OK));
            }
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/auth/logout",
        [authService](const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "JSON body required", "/auth/logout"));
                return;
            }
            std::string refreshToken;
            if (!readJsonField(*json, "refresh_token", refreshToken)) {
                callback(problemJson(drogon::k400BadRequest, "bad_request",
                                     "refresh_token required", "/auth/logout"));
                return;
            }

            authService->logout(refreshToken);
            // 항상 204로 응답해 토큰 존재 여부 누설 방지
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            callback(resp);
        },
        {drogon::Post});

    // 자기 프로필 — user_id, email, display_name (Layout 표시용)
    drogon::app().registerHandler(
        "/me",
        [authService](const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto userId = authService->verifyAccess(std::string(req->getHeader("Authorization")));
            if (!userId) {
                callback(problemJson(drogon::k401Unauthorized, "unauthorized",
                                     "valid Bearer access token required", "/me"));
                return;
            }
            auto cb = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            try {
                auto db = drogon::app().getDbClient("monggle_db");
                db->execSqlAsync(
                    "SELECT email, display_name FROM users WHERE id = ?",
                    [cb, userId](const drogon::orm::Result& rows) {
                        if (rows.size() == 0) {
                            (*cb)(problemJson(drogon::k404NotFound, "user_not_found",
                                              "user record missing", "/me"));
                            return;
                        }
                        Json::Value body;
                        body["user_id"]      = static_cast<Json::Int64>(*userId);
                        body["email"]        = rows[0]["email"].as<std::string>();
                        body["display_name"] = rows[0]["display_name"].as<std::string>();
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
                        resp->setStatusCode(drogon::k200OK);
                        (*cb)(resp);
                    },
                    [cb](const drogon::orm::DrogonDbException& e) {
                        (*cb)(problemJson(drogon::k500InternalServerError, "internal_error",
                                          e.base().what(), "/me"));
                    },
                    *userId);
            } catch (const std::exception& e) {
                (*cb)(problemJson(drogon::k500InternalServerError, "internal_error",
                                  e.what(), "/me"));
            }
        },
        {drogon::Get});
}

} // namespace monggle
