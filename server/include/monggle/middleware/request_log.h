#pragma once

namespace monggle {

// 모든 응답 직전에 한 줄 로그 (액세스 로그). 운영에서는 trace_id 추가 권장.
void installRequestLog();

} // namespace monggle
