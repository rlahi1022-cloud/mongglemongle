#include "monggle/ai/ai_hub_client.h"

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

#include <json/json.h>

namespace monggle {

namespace {

drogon::HttpClientPtr makeClient(const std::string& baseUrl) {
    auto c = drogon::HttpClient::newHttpClient(baseUrl);
    return c;
}

drogon::HttpRequestPtr makeJsonReq(const std::string& path, const Json::Value& body) {
    auto req = drogon::HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath(path);
    return req;
}

} // namespace

AiHubClient::AiHubClient(std::string baseUrl, std::chrono::milliseconds timeout)
    : baseUrl_(std::move(baseUrl)), timeout_(timeout) {}

bool AiHubClient::healthy() {
    auto client = makeClient(baseUrl_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/healthz");
    auto [result, resp] = client->sendRequest(req, std::chrono::duration<double>(timeout_).count());
    if (result != drogon::ReqResult::Ok || !resp) return false;
    return resp->getStatusCode() == drogon::k200OK;
}

std::optional<std::vector<float>> AiHubClient::embed(const std::string& text) {
    try {
        auto client = makeClient(baseUrl_);
        Json::Value body;
        body["text"] = text;
        auto req = makeJsonReq("/embed", body);
        auto [result, resp] = client->sendRequest(req, std::chrono::duration<double>(timeout_).count());
        if (result != drogon::ReqResult::Ok || !resp || resp->getStatusCode() != drogon::k200OK) {
            return std::nullopt;
        }
        auto json = resp->getJsonObject();
        if (!json || !json->isMember("vector") || !(*json)["vector"].isArray()) return std::nullopt;
        std::vector<float> v;
        v.reserve((*json)["vector"].size());
        for (const auto& x : (*json)["vector"]) v.push_back(x.asFloat());
        return v;
    } catch (const std::exception& e) {
        LOG_WARN << "[ai-hub] embed failed: " << e.what();
        return std::nullopt;
    }
}

std::optional<float> AiHubClient::compare(const std::string& a, const std::string& b) {
    try {
        auto client = makeClient(baseUrl_);
        Json::Value body;
        body["a"] = a;
        body["b"] = b;
        auto req = makeJsonReq("/compare", body);
        auto [result, resp] = client->sendRequest(req, std::chrono::duration<double>(timeout_).count());
        if (result != drogon::ReqResult::Ok || !resp || resp->getStatusCode() != drogon::k200OK) {
            return std::nullopt;
        }
        auto json = resp->getJsonObject();
        if (!json || !json->isMember("similarity")) return std::nullopt;
        return (*json)["similarity"].asFloat();
    } catch (const std::exception& e) {
        LOG_WARN << "[ai-hub] compare failed: " << e.what();
        return std::nullopt;
    }
}

} // namespace monggle
