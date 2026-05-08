# E2E (Playwright)

기본 케이스:
- 회원가입 → 로그인 상태 진입 → 헤더 노출
- 백엔드 직접 호출로 사용자 만들고 → 로컬스토리지 주입 → 새 글 작성 → 피드에 노출
- `/healthz` / `/readyz` 200 응답 (스모크)

## 실행

전제: 백엔드 8080 + 프론트 5173 떠있는 상태.

```bash
cd e2e
npm install
npx playwright install chromium
npm test
```

리포트:
```bash
npm run report   # http://127.0.0.1:9323
```

## CI 통합

`.github/workflows/ci.yml`에 추가하려면:
```yaml
e2e:
  runs-on: ubuntu-24.04
  needs: [client-build]
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-node@v4
      with: { node-version: '20' }
    - run: |
        sudo apt-get update -qq && sudo apt-get install -y -qq <백엔드 deps>
        # 백엔드/프론트/AI Hub/MariaDB/Redis 모두 띄운 다음
        cd e2e && npm ci && npx playwright install --with-deps chromium && npm test
```
풀 통합은 분량이 크므로 로컬에서만 우선 검증.
