# 몽글몽글 개발 일지

> 기획서([doc/몽글몽글_기획.pdf](../doc/몽글몽글_기획.pdf))의 어떤 결정을 어떻게 코드로 옮겼는지, 어디서 막혔고 무엇을 의도적으로 보류했는지의 기록.

---

## HTTP 서버 — Drogon 채택

자체 작성한 `Router` + 핸들러 맵 골격이 처음에 있었지만 제거하고 Drogon 1.8 (Ubuntu apt `libdrogon-dev`)로 통합.

- FetchContent로 vendor도 검토했지만 의존성(jsoncpp, c-ares, brotli, hiredis, …)이 너무 많아서 시스템 패키지가 압도적으로 빠름.
- 패키지 8개를 한꺼번에 깔아야 했음 (`libpq-dev`, `libmariadb-dev`, `libsqlite3-dev`, …). Drogon CMake가 모든 DB 백엔드를 강제로 찾는 구조.
- 이후 모든 라우트는 `drogon::app().registerHandler(...)` 또는 `registerHandlerViaRegex(...)` 로 등록.

## 인프라 — 로컬 Docker Compose / 운영 RDS

기획 14장은 운영 환경(AWS RDS Multi-AZ)만 다루고 로컬 개발 환경은 다루지 않음.

- `docker-compose.yml`에 `mariadb:11`, `redis:7-alpine`. healthcheck/볼륨 포함.
- `server/sql/00*.sql`을 `/docker-entrypoint-initdb.d`로 마운트 → 첫 기동 시 9개 테이블 자동 생성.
- env 기반 `AppConfig` (`MONGGLE_DB_*` / `MONGGLE_REDIS_*` / `MONGGLE_JWT_*` / `MONGGLE_MEDIA_STORAGE_ROOT`).
- 막힌 곳: 호스트에 `mariadb-server`가 깔려 있어 3306 포트 충돌. `systemctl stop mariadb && systemctl disable mariadb` 한 번으로 해결.

## 인증 — JWT RS256 + bcrypt

- JWT: `libcpp-jwt-dev` (apt). RS256.
- bcrypt: `libxcrypt`의 `crypt_r()` (`$2b$`). 별도 라이브러리 불필요.
- 토큰 회전: refresh 사용 시 기존 hash 폐기 + 새 페어 발급. `refresh_tokens` 테이블에 SHA-256 해시만 저장.

막힌 곳:
1. 직접 만든 bcrypt salt가 16바이트만 제공해 22자 영역에 `\0` 패딩 → `crypt_r` 거부. `crypt_gensalt_rn("$2b$", cost, ...)`로 교체.
2. `cpp-jwt`의 `verify(true)` 호출이 `JwtService::verify` 멤버와 ADL 충돌 → `jwt::params::verify(true)`로 namespace 명시.
3. nlohmann::json 의 `get<T>()` 파싱 이슈 → `payload.get_claim_value<T>("...")` 패턴으로 우회.

검증 시나리오: signup → login → /me → refresh → logout → 회전된 구토큰 거부 모두 통과.

## 데이터 모델 — 이벤트 소싱이 핵심

`posts`는 "현재 상태 캐시", `post_events`가 "변화의 진실".

- 모든 변경(`created` / `edited` / `visibility_changed` / `deleted` / `media_added` / `restored`)을 `post_events`에 누적.
- `Drogon::DbClient::newTransaction()` 으로 posts UPDATE와 post_events INSERT를 원자 묶음.
- soft delete: `posts.deleted_at` NULL 필터로 모든 SELECT/UPDATE 제한. 하드 삭제 안 함 → 이벤트 이력 보존.
- 'restored' 이벤트 추가는 시점 복원 화면의 "이 글 살리기" 버튼을 위한 후속 마이그레이션 (`004_post_events_restored.sql`).

## 시점 복원 알고리즘 (기획 10.5)

기획서의 의사코드를 그대로 옮김.

```
restore_state(user_id, target_time):
  1) snapshots에서 target_time 이전 가장 가까운 스냅샷 로드 (없으면 빈 상태)
  2) cursor 이후 ~ target_time까지의 post_events를 apply_event로 누적
```

`apply_event`:
- `created` → `posts[id] = {body, visibility, deleted=false}`
- `edited` → `posts[id].body = payload.body`
- `visibility_changed` → `posts[id].visibility = payload.to`
- `deleted` → `posts[id].deleted = true`
- `restored` → `posts[id].deleted = false`
- `media_added` → `lastEventId`만 갱신 (본문 영향 없음)

의도된 단순화: 스냅샷 워커는 미구현. 빈 `snapshots` 테이블에서 처음부터 재생. 데이터 폭증 전까지 충분히 실용적.

검증: 같은 글의 created → edited → visibility_changed → deleted 5개 이벤트가 각 시점별로 정확히 복원됨을 확인 (body, visibility, deleted 모두 그 시점 그대로).

## 권한 — 두 매트릭스 분리 (기획 8장)

조회 권한과 다운로드 권한을 별도로 둠.

| | 본인 | 팔로워 | 타인 |
|---|---|---|---|
| visibility=`public` | OK | OK | OK |
| visibility=`friends` | OK | OK | 차단 |
| visibility=`private` | OK | 차단 | 차단 |

다운로드는 위에 더해 `download_policy` (`owner_only` / `followers` / `public_allowed`) 가 추가로 통과해야 함.

`PostsService`에 `FollowsService`를 주입해 `friends` 분기를 `isFollower(viewer, author)`로 판정. unfollow 즉시 다음 요청부터 권한 회수되는 것까지 검증.

## 친구 / 피드 — Pull 전략

기획 12.4의 Push/Pull Hybrid 중 MVP는 **Pull only**.

- 작성 시 fanout 없음, 읽을 때 SQL JOIN으로 본인 + 팔로우한 사람의 (public|friends) 글을 합치기.
- Push 캐시는 Redis 안정화 후로 미룸.
- 팔로워 1k 이하라면 성능 차이 무시할 만함.

## 미디어 — 의도된 단축 (S3 → 로컬 FS)

기획 6장은 "메인서버는 미디어 바이트를 받지 않음 + S3 직접 업로드 + 서명 URL"이지만 MVP에서는 **로컬 파일시스템 + 백엔드 경유 업로드**로 단축.

이유:
- MinIO + aws-sdk-cpp + SigV4 정확 구현은 수 시간이 더 들고, MVP 화면 데모로 가는 길이 막힘.
- 트레이드오프: 백엔드 메모리/네트워크에 미디어 바이트가 잠깐 머무름. 운영 부하 시점 전까지 OK.

구현:
- 사진: OpenCV `imread` → 200/800 너비 비율 유지 jpg 출력.
- 영상: 인코딩 안 함. ffmpeg 시스템 호출(`-frames:v 1`)로 첫 프레임 PNG poster만 생성.
- 프로필 아바타: OpenCV로 정사각형 중앙 크롭 + 256x256 jpg.
- 권한 두 매트릭스 분리 (위 참고).

막힌 곳:
- Drogon 1.8.7의 `HttpFile::getContentTypeString()` 부재 → 파일명 확장자에서 mime 추론.
- `.gitignore`에 `media/`만 적었더니 `server/.../media/` 폴더까지 무시되어 헤더/구현이 누락된 채로 commit. anchor `/media/`로 root 한정.

## 검색 — LIKE 채택

MariaDB 11에 한글 ngram FULLTEXT가 없어서 `ngram` 파서 ALTER가 실패 → MVP는 `LIKE %q%`. `posts.body`에 FULLTEXT 인덱스는 정의해뒀으나 한글에선 무용지물이라 LIKE로 우회.

운영 규모 도달 시 Elasticsearch/Meilisearch 또는 AI 임베딩 검색으로 대체.

## CORS + Rate Limit + Redis 보류

- **CORS**: Drogon `registerSyncAdvice`로 OPTIONS preflight 가로채기 + `registerPreSendingAdvice`로 모든 응답에 `Allow-Origin/Vary/Credentials` 부착. 화이트리스트(`localhost:5173/3000`).
- **RateLimiter**: 메모리 슬라이딩 윈도우. 정책은 기획 12.8 그대로 (`auth_login` 5/min IP, `post_create` 10/min user, `search` 30/min user).
- **Redis 보류**: `/readyz` Redis ping 도입 시도 → Drogon 1.8.7 (Ubuntu) `RedisClient` 첫 PING 호출에서 process segfault 재현. 디버깅 비용 큼. `createRedisClient` 호출 자체를 일단 제거. `/readyz`는 DB-only, `redis="skipped"`로 명시. L2 캐시·Push fanout도 같은 사유로 후속.

## 액세스 로그 — 운영 디버깅 도움

화면 디버깅 중 발견: 백엔드는 시작 로그 외 무음이라 어디서 막혔는지 모름.

- `installRequestLog()`: `registerPreSendingAdvice`로 모든 응답 직전에 `"POST /auth/login -> 200 peer=..."` 한 줄.
- 운영에선 `trace_id` 추가 권장 (현재는 없음).

## 프론트엔드 — Vite + React + TS + Tailwind + shadcn

스택:
- Node 18에서 최신 `create-vite`가 `util.styleText` 못 찾아 실패 → `npm create vite@5`로 우회.
- shadcn CLI 안 쓰고 컴포넌트(button/input/card/textarea/label) 직접 작성. 외부 의존성 최소화.
- React Router v7, TypeScript strict 통과.

구성:
- `src/api/client.ts`: fetch 래퍼. access 자동, 401 시 refresh 1회 재시도, problem+json → `ApiError` 매핑.
- `AuthContext` + `ProtectedRoute`.
- 6개 페이지: `/login`, `/signup`, `/feed`(composer 포함), `/me/timeline`, `/snapshot`(datetime-local), `/search`.
- 미디어 표시: `PostCard` 안에서 `useEffect`로 `GET /posts/{id}/media` fetch → 썸네일/포스터 표시.
- 회원가입 비밀번호 재확인: `passwordMismatch` 메모 + 일치 시 `✓` / 불일치 시 빨간 테두리.
- 글 본문 1000자 제한 + 카운터 (90% 넘으면 destructive 색).

## 디자인 — 저녁하늘 + 구름 마스코트

피드백 누적으로 정착한 톤:
- body 5단 그라데이션 (옅은 하늘 → 노을 → 남색 → 깊은 밤).
- `starfield` + `starfield-extra` 두 겹 별 + `twinkle` 애니메이션 (3s/5s 시차).
- 좌측 흰 사이드바 (마스코트 + 메뉴 + 친구 박스 + 프로필 아바타).
- borderRadius 토큰을 1rem으로 키워서 모든 카드/입력/버튼이 동글동글.
- 사이드바 sticky: 외부 div가 `h-screen overflow-hidden`, main만 `overflow-y-auto`.
- 페이지 헤더는 그라데이션 위에서 안 보여서 `cloud-card` 칩으로 감싸 가독성 확보.

## 의도적으로 남겨둔 것 (follow-up)

| 항목 | 사유 |
|---|---|
| S3/MinIO + 서명 URL (기획 6장) | MVP 단축, 로컬 FS로 대체. 운영 진입 시점에 `aws-sdk-cpp` 도입 |
| Redis L2 캐시 + Push fanout (기획 12) | Drogon `RedisClient` segfault 해결 후. `hiredis` 직접도 옵션 |
| AI 허브 + 임베딩 검색 (기획 5장) | 외부 비용 분리. BGE-m3 도입 시 `embeddings` 테이블 그대로 사용 가능 |
| EventBus 워터마크 (기획 11.3) | 본격 EventBus 미구현. Drogon loop 기반 비동기로 충분한 동안 유보 |
| AWS 인프라 자동화 + 부하 테스트 (기획 14·15) | 화면 시연 후 단계 |
| 사용자 프로필 페이지 / 타인 타임라인 UI | 백엔드 준비됨. `/users/{id}` 화면만 추가하면 됨 |
| 반응형(모바일) 사이드바 | 데스크톱 우선. 햄버거 메뉴 토글 후속 |

## 기술 결정 한 줄 요약

| 영역 | MVP 선택 | 운영용 정석 |
|---|---|---|
| HTTP | Drogon | (그대로) |
| DB | MariaDB Docker | RDS Multi-AZ |
| 캐시 | (없음) | Redis ElastiCache |
| 미디어 저장 | 로컬 FS | S3 + CloudFront |
| 미디어 업로드 | 백엔드 경유 multipart | 클라이언트 → S3 직접 + 서명 URL |
| 검색 | LIKE | Elasticsearch / AI 임베딩 |
| 팬아웃 | Pull on read | Push/Pull Hybrid (1k 임계치) |
| 인증 | JWT RS256 | (그대로) |
| 인프라 | Docker Compose | EC2 × 2 + ALB + Multi-AZ |
| 부하 검증 | (없음) | k6/wrk 시나리오 4종 |
