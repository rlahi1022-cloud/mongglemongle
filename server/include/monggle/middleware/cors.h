#pragma once

#include <string>
#include <vector>

namespace monggle {

struct CorsConfig {
    std::vector<std::string> allowedOrigins;   // 정확 일치 (와일드카드는 보안상 비권장)
    std::string              allowedMethods;   // "GET, POST, PATCH, DELETE, OPTIONS"
    std::string              allowedHeaders;   // "Authorization, Content-Type"
    std::string              exposedHeaders;   // "Content-Disposition"
    std::string              maxAge;           // preflight 캐시 초
    bool                     allowCredentials;
};

// drogon::app() 에 OPTIONS 가로채기 + 응답 헤더 추가 advice 등록
void installCors(const CorsConfig& cfg);

CorsConfig defaultDevCors();

} // namespace monggle
