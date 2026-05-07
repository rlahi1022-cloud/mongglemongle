#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace monggle {

// AI 허브 (FastAPI Python 서비스) 호출 클라이언트.
// MVP에서는 의미 검색 시점에 즉석 임베딩 → DB 풀스캔 코사인 유사도 비교.
// 운영에서는 작성 시점에 사전 임베딩 → embeddings 테이블 + ivfflat 인덱스.
class AiHubClient {
public:
    explicit AiHubClient(std::string baseUrl, std::chrono::milliseconds timeout);

    // 텍스트 → 임베딩 벡터 (실패 시 nullopt). circuit breaker는 후속.
    std::optional<std::vector<float>> embed(const std::string& text);

    // 두 텍스트 간 코사인 유사도 (실패 시 nullopt)
    std::optional<float> compare(const std::string& a, const std::string& b);

    bool healthy();

private:
    std::string                baseUrl_;
    std::chrono::milliseconds  timeout_;
};

} // namespace monggle
