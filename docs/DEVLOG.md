# 몽글몽글 개발 일지

> 기획서([doc/몽글몽글_기획.pdf](../doc/몽글몽글_기획.pdf))의 어떤 결정을 어떻게 코드로 옮겼는지, 막힌 곳은 어디고 무엇을 보류했는지의 기록.

---

## Day 0 — 출발선 점검

빈 스켈레톤(`Initial project skeleton`) 위에서 시작. 기획서 40페이지 통독.
- **무엇이 있었나**: `include/`, `src/` 평면 구조의 C++ 파일 7개. 모두 `std::cout`만 호출하는 골격.
- **무엇이 없었나**: 빌드 시스템(있긴 한데 못 돎), DB, 인증, 라우팅, 클라이언트, 테스트.
- **결정**: 디렉터리를 `server/` / `client/`로 분리. 도메인별 폴더(`auth/`, `posts/`, …)로 묶음. `.gitignore` 추가.

→ `47fb3d4 Restructure project into server/client layout`

---

## Day 1 — HTTP 서버

**선택**: Drogon 1.8.7 (Ubuntu 24.04 `libdrogon-dev`).
- FetchContent로 vendor도 검토했지만 의존성(jsoncpp, c-ares, brotli, hiredis 등)이 너무 많아서 시스템 패키지가 압도적으로 빠름.
- 자체 작성한 `Router`/핸들러 맵은 제거. Drogon `registerHandler`로 일원화.

**처음 막힌 곳**: 패키지 8개를 한꺼번에 깔아야 했음 (`libpq-dev`, `libmariadb-dev`, `libsqlite3-dev`, `libbrotli-dev`, …). Drogon CMake가 모든 DB 백엔드를 강제로 찾아서 그렇다.

→ `505f8d1 Integrate Drogon HTTP server, drop custom Router`

---

## Day 2 — 인프라(MariaDB + Redis) Docker Compose

**결정**: 로컬은 Docker Compose, 운영은 AWS RDS (기획 14장).
- `docker-compose.yml`에 `mariadb:11`, `redis:7-alpine` healthcheck/볼륨 포함.
- `server/sql/001_init.sql` (기획 7·9장 9개 테이블)을 `/docker-entrypoint-initdb.d`로 마운트해 첫 기동 시 자동 생성.
- env 기반 `AppConfig`, `MONGGLE_DB_*` / `MONGGLE_REDIS_*`.
- `/healthz`, `/readyz` 엔드포인트.

**처음 막힌 곳**: 호스트에 `mariadb-server`가 깔려 있어 3306 포트 충돌 → `systemctl stop mariadb && systemctl disable mariadb`.

→ `803c326 Add Docker Compose infra (MariaDB+Redis), schema, env-driven config`

---

## Day 3 — 인증 (JWT RS256 + bcrypt)

**선택**:
- JWT: `libcpp-jwt-dev` (apt). RS256.
- bcrypt: `libxcrypt`의 `crypt_r()` (`$2b$`). 별도 라이브러리 불필요.
- 토큰 회전: refresh 사용 시 기존 hash 폐기 + 새 페어 발급. `refresh_tokens` 테이블에 SHA-256 해시만 저장.

**처음 막힌 곳**:
1. 직접 만든 bcrypt salt가 16바이트만 제공해 22자 영역에 `\0` 패딩 → `crypt_r` 거부. `crypt_gensalt_rn("$2b$", cost, ...)`로 교체.
2. `cpp-jwt` 의 `verify(true)` 호출이 `JwtService::verify` 멤버와 ADL 충돌 → `jwt::params::verify(true)`로 명시적 namespace.
3. nlohmann::json 의 `get<T>()` 파싱 이슈 → `payload.get_claim_value<T>("...")` 패턴으로 우회.

검증: signup → login → /me → refresh → logout → 회전된 구토큰 거부 모두 통과.

→ `da6fa10 C1: JWT(RS256) 인증 — signup/login/refresh/logout 구현`

---

## Day 4 — posts CRUD + 이벤트 소싱

**결정**: `posts`는 "현재 상태 캐시", `post_events`가 "변화의 진실". 모든 변경(create / edit / visibility_change / delete / media_added)을 이벤트로 누적. `Drogon::DbClient::newTransaction()` 으로 posts UPDATE와 post_events INSERT를 원자 묶음.

**의도된 단순화**: visibility `friends` 분기는 follows 미구현 시점이라 author만 통과시키고, C4에서 제대로 채움.

→ `636b3c9 C2: posts CRUD + post_events 이벤트 소싱 + 권한 매트릭스(부분)`

---

## Day 5 — 시점 복원 + 검색

기획 10.5의 `restore_state(user_id, target_time)`을 그대로 옮김.
1. `snapshots`에서 `target_time` 이전 가장 가까운 스냅샷 로드 (없으면 빈 상태).
2. `cursor` 이후 ~ `target_time`까지의 `post_events`를 `apply_event`로 누적.

**의도된 단순화**: 스냅샷 워커는 미구현. 빈 `snapshots` 테이블에서 처음부터 재생. 데이터가 폭증하기 전까지 충분히 실용적.

검증: 같은 글의 created → edited → visibility_changed → deleted 5개 이벤트가 각 시점별로 정확히 복원됨을 확인 (body, visibility, deleted 모두 그 시점 그대로).

**검색**: MariaDB 11에 한글 ngram FULLTEXT가 없어서 `ngram` 파서 ALTER가 실패 → MVP는 `LIKE %q%`. 운영은 Elasticsearch/Meilisearch 또는 AI 임베딩.

→ `9df31be C3: 시점 복원 알고리즘 + 키워드 검색 (MVP)`

---

## Day 6 — 친구 + 피드

**결정**: Push/Pull Hybrid (기획 12.4) 중 MVP는 **Pull only**. 작성 시 fanout 없음, 읽을 때 SQL JOIN으로 본인 + 팔로우한 사람의 (public|friends) 글 합치기. Push 캐시는 Redis 안정화 후.

`PostsService`에 `FollowsService`를 주입해 `friends` visibility를 `isFollower(viewer, author)`로 판정. unfollow 즉시 다음 요청부터 권한 회수되는 것까지 검증.

→ `efbbcb2 C4: follows + friends visibility + /me/feed (Pull)`

---

## Day 7 — 미디어 (의도된 단축)

**결정**: 기획 6장은 "메인서버는 미디어 바이트를 받지 않음 + S3 직접 업로드 + 서명 URL"이지만 MVP에서는 **로컬 파일시스템 + 백엔드 경유 업로드**로 단축.
- 이유: MinIO + aws-sdk-cpp + SigV4 정확 구현은 수 시간이 더 들고, MVP 화면 데모로 가는 길이 막힘.
- 트레이드오프: 백엔드 메모리/네트워크에 미디어 바이트가 잠깐 머무름. 운영 부하 시점 전까지 OK.

**구현**:
- 사진: OpenCV로 `imread` → 200/800 너비 비율 유지 jpg 출력.
- 영상: 인코딩 안 함. ffmpeg 시스템 호출(`-frames:v 1`)로 첫 프레임 PNG poster만 생성.
- 권한 두 매트릭스 분리: `canViewPost(visibility)` + `canDownload(visibility ∧ download_policy)`.

**처음 막힌 곳**:
- Drogon 1.8.7의 `HttpFile::getContentTypeString()` 부재 → 파일명 확장자에서 mime 추론.
- `.gitignore`에 `media/`만 적었더니 `server/.../media/` 폴더까지 무시되어 헤더/구현이 누락된 채로 commit. anchor `/media/`로 root 한정.

→ `f6dd1bd C5: 미디어 — 멀티파트 업로드, OpenCV 썸네일, ffmpeg 포스터, 권한 분리`
→ `59266a3 C5 fix: include MediaService source files`

---

## Day 8 — 운영 안정성 (CORS + Rate Limit) + Redis 보류

**구현**:
- CORS: Drogon `registerSyncAdvice`로 OPTIONS preflight 가로채기 + `registerPreSendingAdvice`로 모든 응답에 `Allow-Origin/Vary/Credentials` 부착. 화이트리스트(`localhost:5173/3000`).
- RateLimiter: 메모리 슬라이딩 윈도우. 정책은 기획 12.8 그대로 (`auth_login` 5/min IP, `post_create` 10/min user, `search` 30/min user).

**보류**: `/readyz` Redis ping 도입 시도 → Drogon 1.8.7 (Ubuntu) `RedisClient` 첫 PING 호출에서 process segfault 재현. 디버깅 비용 大. `createRedisClient` 호출 자체를 일단 제거. `/readyz`는 DB-only, `redis="skipped"`로 명시. 후속 작업.

→ `bf746a8 C6: CORS + Rate Limit + /readyz 정리 (Redis는 후속)`

---

## Day 9 — React 클라이언트 골격

**스택**: React 18 + Vite 5 + TypeScript + Tailwind 3 + shadcn/ui (직접 vendor).
- Node 18에서 최신 `create-vite`가 `util.styleText` 못 찾아 실패 → `npm create vite@5`로 우회.
- shadcn CLI 안 쓰고 컴포넌트(button/input/card/textarea/label) 직접 작성.

**구성**:
- `src/api/client.ts`: fetch 래퍼. access 자동, 401 시 refresh 1회 재시도, problem+json → `ApiError` 매핑.
- `AuthContext` + `ProtectedRoute`.
- 6개 페이지: `/login`, `/signup`, `/feed`(composer 포함), `/me/timeline`, `/snapshot`(datetime-local), `/search`.

→ `7d775ea client: React + Vite + Tailwind + shadcn/ui MVP 화면`

---

## Day 10 — 운영 디버깅 도움

화면에서 `user #5`만 보이고 가입 시 입력한 이름이 안 노출. 백엔드는 시작 로그 외 무음.
- `/me` 응답에 `email` + `display_name` + `has_avatar` 추가.
- `installRequestLog()`: 모든 응답 직전에 `"POST /auth/login -> 200 peer=..."` 한 줄.
- `AuthContext`가 마운트/로그인 직후 `me.whoami()`로 프로필 갱신.

→ `8329eb1 feat: 백엔드 액세스 로그 + /me 에 display_name/email 추가`

---

## Day 11 — 디자인 (저녁하늘 + 마스코트)

피드백: "하늘색 + 남색 그라데이션, 별 반짝, 동글동글, 마스코트 좌상단".
- body 5단 그라데이션 (옅은 하늘 → 노을 → 남색 → 깊은 밤).
- `starfield` + `starfield-extra` 두 겹 별 + `twinkle` 애니메이션 (3s/5s 시차).
- 좌측 사이드바 (마스코트 + 메뉴 + 친구 박스 + 프로필).
- borderRadius 토큰 키워서(`--radius: 1rem`) 모든 카드/입력/버튼 동글동글.

이후 추가 피드백: "흰색 비중 늘려라, 마스코트 흰 배경 안 들키게" → 사이드바를 다크에서 흰색으로, 그 위에 마스코트가 자연스럽게 녹아듦.
또 이후: "사이드바 sticky 안 되어서 친구박스가 안 보임" → 외부 div를 `min-h-screen`에서 `h-screen overflow-hidden`으로, main만 `overflow-y-auto`.

→ `8ff0c0b client: 저녁하늘 + 구름 마스코트 테마, 좌측 사이드바 전면 개편`

---

## Day 12 — 사용자 시나리오 마무리

피드백 누적:
- 첨부한 사진이 피드 카드에 안 보임 → `GET /posts/{id}/media` 엔드포인트 + `PostCard` 안에서 `useEffect`로 fetch + 썸네일 표시.
- 회원가입 비밀번호 재확인 → `passwordMismatch` 메모 + 일치 시 `✓` 안내 + 불일치 시 빨간 테두리.
- 프로필 이미지 → `users.avatar_path` 컬럼 + `ProfileService`(OpenCV로 정사각형 크롭 + 256x256 jpg) + `PUT /me/avatar` + `GET /users/{id}/avatar` (공개). 사이드바 아바타 클릭으로 파일 선택.
- 글자수 제한 → 프론트 `POST_BODY_MAX = 1000`, 백엔드 `body.size() > 3000` 거부 (UTF-8 한글 ~3바이트 기준).
- favicon/title → `/mascot.png` 사용, 타이틀 "몽글몽글" 한 마디.

---

## 의도적으로 남겨둔 항목 (follow-up)

1. **S3/MinIO + 서명 URL** — 기획 6장의 정석. 운영 진입 시점에 `aws-sdk-cpp` 도입.
2. **Redis L2 캐시 + Push fanout** — Drogon `RedisClient` segfault 해결 후. `hiredis` 직접도 옵션.
3. **AI 허브 + 임베딩 검색** — MVP 비용 분리 결정. BGE-m3 도입 시 `embeddings` 테이블 그대로 사용 가능.
4. **EventBus 워터마크 백프레셔** — 본격 EventBus 미구현. Drogon loop 기반 비동기로 충분한 동안 유보.
5. **AWS 인프라 자동화 + 부하 테스트(k6/wrk)** — 기획 14·15장. 클라이언트 시연 후 단계.
6. **사용자 프로필 페이지 / 타인 타임라인 UI** — 백엔드는 준비됨. `/users/{id}` 화면만 추가하면 됨.
7. **반응형(모바일) 사이드바** — 햄버거 메뉴 토글.

## 기술 결정 한 줄 요약

| 영역 | MVP 선택 | 운영용 정석 |
|---|---|---|
| HTTP | Drogon | (그대로) |
| DB | MariaDB Docker | RDS Multi-AZ |
| 캐시 | (없음) | Redis ElastiCache |
| 미디어 저장 | 로컬 FS | S3 + CloudFront |
| 미디어 업로드 | 백엔드 경유 multipart | 클라이언트 → S3 직접 + 서명 URL |
| 검색 | LIKE | Elasticsearch/AI 임베딩 |
| 팬아웃 | Pull on read | Push/Pull Hybrid (1k 임계치) |
| 인증 | JWT RS256 | (그대로) |
| 인프라 | Docker Compose | EC2 × 2 + ALB + Multi-AZ |
| 부하 검증 | (없음) | k6/wrk 시나리오 4종 |
