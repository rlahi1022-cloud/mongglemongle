# 부하 테스트 + 관측

## Grafana / Prometheus

```bash
sg docker -c "docker compose --profile observability up -d prometheus grafana"
```

- Prometheus: http://127.0.0.1:9090
- Grafana:    http://127.0.0.1:3001  (anonymous Viewer 활성화, admin pw `monggle_dev`)

대시보드 `Monggle 백엔드 운영` 자동 provisioning. 백엔드는 호스트에서 `:8080`으로 떠있어야 하고, `host.docker.internal` extra_hosts로 컨테이너에서 접근됩니다.

## k6 부하 테스트 (피드 핫 패스)

```bash
sg docker -c "docker run --rm -i --network host grafana/k6 run \
  -e MONGGLE_BEARER='<유효한 access token>' \
  - < loadtest/feed_smoke.js"
```

스테이지: 30 VU 30s warmup → 80 VU 1m sustained → ramp down. P95 < 500ms / 실패율 < 1% threshold.

테스트 진행 중 Grafana 대시보드에서 확인할 패널:
- **RPS** — 분당 요청 수
- **응답 시간 p50/p95/p99**
- **캐시 hit ratio** — 캐시 도입 효과의 핵심 수치
- **Rate-limit 거부율** — 80 VU 가 30/min/user 한계 초과 시 점등
- **Redis up / Snapshot 워커**
