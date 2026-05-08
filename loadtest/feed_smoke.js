// k6 부하 테스트 — 피드 핫 패스 (캐시 hit/miss + rate limiter)
//
//   docker run --rm -i --network host grafana/k6 run - < loadtest/feed_smoke.js
//
// 단계:
//   1) 30 VU × 30s   warmup (캐시 채우기)
//   2) 80 VU × 1m    sustained
//   3) ramp down

import http from 'k6/http';
import { check, sleep } from 'k6';

const BASE = __ENV.BASE || 'http://127.0.0.1:8080';
const TOKEN = __ENV.MONGGLE_BEARER || '';   // 미설정 시 401만 보고 캐시는 안 검증됨

export const options = {
    stages: [
        { duration: '30s', target: 30 },
        { duration: '1m',  target: 80 },
        { duration: '20s', target: 0  },
    ],
    thresholds: {
        // P95 응답 < 500ms, 실패율 < 1%
        http_req_duration: ['p(95)<500'],
        http_req_failed:   ['rate<0.01'],
    },
};

const headers = TOKEN ? { Authorization: `Bearer ${TOKEN}` } : {};

export default function () {
    const r1 = http.get(`${BASE}/me/feed`, { headers });
    check(r1, {
        'feed status is 200 or 401': (r) => r.status === 200 || r.status === 401,
    });

    // 헬스 체크 — 항상 빠르게 응답해야 함
    const r2 = http.get(`${BASE}/healthz`);
    check(r2, { 'healthz 200': (r) => r.status === 200 });

    sleep(0.2);
}
