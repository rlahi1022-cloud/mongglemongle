# 몽글몽글 (Monggle Monggle)

<p align="center">
  <img src="docs/마스코트.png" alt="몽글몽글 마스코트" width="180" />
</p>

<p align="center">
  <em>지나간 기록을 꺼내 오늘의 글로 잇는 시스템</em>
</p>

---

## 무엇을 만들었나

매일 무언가를 만드는 사람(개발자/디자이너/학습자)이 자기 글·사진·영상을 **시점과 함께** 저장해 두고, 임의의 과거 시점으로 돌아가 다시 꺼내 볼 수 있게 하는 커뮤니티 플랫폼입니다.

핵심 어필 축 3가지:
- **시점 복원** — 이벤트 소싱 + 스냅샷으로 임의 시점의 사용자 상태를 재구성.
- **자원 설계** — L1/L2 캐시, Redis 장애 대응, 미디어 처리 경로를 코드로 구현.
- **트래픽 처리** — Pull 기반 피드, 캐시 계층, 미디어 조회/다운로드 권한 처리를 다룸.

→ 자세한 설계: [docs/몽글몽글_기획.pdf](docs/몽글몽글_기획.pdf)
→ 어떻게 코드로 옮겼는지: [docs/DEVLOG.md](docs/DEVLOG.md)
→ 네이버 카페 원문 수집 도구: [docs/네이버카페_원문추출기_구현정리.md](docs/네이버카페_원문추출기_구현정리.md)

---

## 구현된 기능

### 인증 & 보안
- **JWT RS256** access(15m) + refresh(14d) 페어, refresh 회전 시 기존 hash 폐기
- **bcrypt** 해싱 (libxcrypt `crypt_gensalt_rn` / `crypt_r`, $2b$ 저장)
- **Rate limiting**: 로그인 5/min/IP, 글 작성 10/min/user, 검색 30/min/user
- **CORS preflight** (PUT 포함, 아바타 업로드 호환)
- 비밀번호 변경 전 사전 확인 게이트 (`/me/verify-password`)

### 글 & 이벤트 소싱
- `posts`(현재 상태) + `post_events`(변화의 진실) 이원 구조 — 모든 변화는 이벤트로 적재
- 이벤트 타입: `created`, `edited`, `visibility_changed`, `deleted`, `media_added`, `restored`
- 트랜잭션으로 `posts` UPDATE와 `post_events` INSERT 원자성 보장
- **Soft delete** (`deleted_at` NULL 필터) — 하드 삭제 금지, 이력 보존
- 카테고리: `feed` / `devlog` 분리 (타임라인/조회 시 필터링)

### 시점 복원 (Snapshot)
- `/me/snapshot?at=ISO8601` — 임의 시점의 사용자 글 상태 재구성
- target_time 이전 가장 가까운 스냅샷 로드 → 이후 `post_events` 순차 재생 → 최종 상태 계산
- 삭제된 글을 그 시점 기준으로 살리는 "복원" 액션 제공 (restored 이벤트 적재)

### 검색
- `/me/search?q=...` LIKE 키워드 검색
- AI Hub가 살아있으면 임베딩 유사도 결과를 키워드 결과와 혼합

### 친구 / 팔로우 / 피드
- 단방향 follow (`/users/{id}/follow` POST/DELETE)
- 팔로워/팔로잉 목록 (`/me/followers`, `/me/following`)
- **Pull 기반 피드** — 작성 시 fanout 없음, 읽을 때 SQL JOIN으로 본인 + 팔로우의 (public|friends) 글 합치기
- **2계층 피드 캐시 (L1 in-process + L2 Redis)** — Redis 장애 시 L1-only로 동작

### 미디어
- multipart 업로드 (`/posts/{id}/media`)
- 이미지: OpenCV로 200/800 너비 비율 유지 JPEG, 썸네일 자동 생성
- 영상: ffmpeg로 첫 프레임 PNG poster 생성 (원본은 인코딩 없이 보관)
- 저장소: MinIO/S3 호환 백엔드와 로컬 FS 대체 경로 제공
- **다운로드 정책 매트릭스**: `owner_only` / `followers` / `public_allowed` — visibility와 분리해서 운영

### 댓글 / 알림 / 차단
- 댓글: `GET/POST /posts/{id}/comments`, `DELETE /comments/{id}` (본인 댓글만)
- 알림: 댓글/팔로우 시 자동 생성, 미읽음 카운트, 전체 읽음 처리, SSE 스트림(`/me/notifications/stream`)
- 차단: 차단된 사용자의 글/팔로우/댓글 제외, 본인 또는 차단된 사용자만 해제

### 프로필
- 아바타 업로드 → OpenCV로 정사각형 256×256 JPEG 변환 (`PUT /me/avatar`)
- 표시 이름 변경 즉시 반영 (모든 카드/사이드바에 동기화)
- 비밀번호 변경 (사전 확인 게이트 통과 시)
- 공개 아바타 조회 (`GET /users/{id}/avatar`, 없으면 프론트에서 첫 글자 표시)

### 개발일지 (DevLog)
- 피드 글을 **근거**로 선택해 개발일지 본문 생성
- 네이버 카페 원문 추출기가 만든 Markdown 파일 다중 import → 근거 변환
- GitHub 커밋 import (`owner/repo` 또는 URL + 날짜 범위)
- 형식 선택: **공부형** (개념/배운 점/새롭게 느낀 점) 또는 **당일 개발 경험**
- 공부형은 입력된 개념에서 기술 용어를 추출, 정의 근거 부족 항목은 추가 확인 대상으로 표시
- 개발일지는 일반 피드보다 긴 본문 허용, 발행 시 공개 범위 직접 선택

### 캐시 계층
- **L1**: in-process `TtlCache` — `unordered_map` + `std::mutex`, 30s TTL
- **L2**: Redis (`redis-plus-plus`, vcpkg manifest로 관리) — `SET ... EX`로 TTL 적재, prefix invalidate는 `SCAN` + `UNLINK`
- 글 작성 시 작성자 본인의 `feed:{userId}:` 캐시를 무효화하고, 팔로워 피드는 짧은 TTL로 자연 만료
- **Graceful degradation**: Redis 다운 → L2가 자동으로 `healthy=false`로 떨어지고 L1 단독 모드 지속, `/readyz`의 `redis` 필드가 `down`으로 노출되지만 서비스는 끊기지 않음

### EventBus
- in-process pub/sub — 동기 디스패치, subscriber 예외 격리
- 토픽 상수: `kPostCreated`, `kPostEdited`, `kPostDeleted`, `kMediaUploaded`, `kFollowAdded`, `kCommentAdded`

### AI 허브
- Python FastAPI 서비스로 분리 (`ai-hub/app.py`)
- 글 본문 임베딩 → `embeddings` 테이블 적재
- 검색 시 LIKE 결과와 임베딩 유사도 결과 혼합
- 개발일지 본문 생성 API 제공
- 모델 로드 실패 시 대체 임베딩으로 서비스 지속

### 운영
- `/healthz` — 프로세스 헬스
- `/readyz` — DB + Redis ping (Redis 끊겨도 L1으로 동작 보장)
- `/metrics` — Prometheus 텍스트 포맷의 HTTP/cache/ratelimit/redis 지표
- 모든 응답에 액세스 로그 한 줄 (`monggle-access.log`)

---

## 기술 스택

| 레이어 | 선택 |
|---|---|
| 백엔드 | C++, Drogon |
| 인증 | JWT RS256 (cpp-jwt), bcrypt (libxcrypt) |
| DB | MariaDB (Drogon ORM, 트랜잭션) |
| 캐시 | L1 in-process TTL + L2 Redis (redis-plus-plus, vcpkg) |
| 미디어 | OpenCV (이미지 리사이즈/썸네일) + ffmpeg (영상 첫 프레임) + MinIO/S3 호환 저장소 |
| 클라이언트 | React + Vite + TypeScript + Tailwind + shadcn/ui |
| 로컬 인프라 | Docker Compose (MariaDB + Redis + MinIO + AI Hub) |
| 운영 인프라 | AWS RDS + ElastiCache + S3 + CloudFront Terraform 구성 |
| AI 허브 | Python FastAPI + sentence-transformers (BGE-m3) |
| 의존성 관리 | apt (시스템 라이브러리) + vcpkg manifest (redis-plus-plus) |

---

## 디렉터리

```
.
├── docs/                 ← 기획서 PDF, 마스코트, 시스템 다이어그램, DEVLOG, 네이버카페 추출기 메모
├── docker-compose.yml    ← MariaDB, Redis, MinIO, AI Hub
├── vcpkg.json            ← redis-plus-plus manifest
├── server/               ← C++ 백엔드
│   ├── include/monggle/
│   │   ├── auth/         JwtService, PasswordService, AuthService
│   │   ├── cache/        ICache, TtlCache(L1), RedisCache(L2), LayeredCache
│   │   ├── follows/      FollowsService
│   │   ├── blocks/       BlocksService
│   │   ├── comments/     CommentsService
│   │   ├── notifications/NotificationsService
│   │   ├── media/        MediaService (사진/영상)
│   │   ├── middleware/   cors, rate_limiter, request_log
│   │   ├── posts/        PostsService, SnapshotService
│   │   ├── profile/      ProfileService (아바타)
│   │   ├── event/        EventBus
│   │   └── router/       routes.h
│   ├── src/              위 헤더의 구현
│   └── sql/              초기 스키마 + 마이그레이션
├── ai-hub/               ← Python FastAPI 임베딩 서비스
├── client/               ← React 프론트엔드
│   └── src/
│       ├── api/          백엔드 fetch 래퍼 (auth 자동, 401 refresh 회전)
│       ├── auth/         AuthContext, ProtectedRoute
│       ├── components/   Layout, PostCard, EditPostDialog, DevlogDraftDialog, FriendsBox, ...
│       └── pages/        Login, Signup, Feed, MyTimeline, Snapshot, Search, Devlogs, Profile
├── infra/terraform/      ← AWS RDS/ElastiCache/S3/CloudFront 모듈
├── tests/                ← C++ 테스트 스켈레톤
├── scripts/gen_dev_jwt_keys.sh
└── CMakeLists.txt
```

---

## 화면

| 영역 | 설명 |
|---|---|
| 상단 내비게이션 | 마스코트 + 메뉴(피드/내 글/시점 복원/검색) + 알림 + 프로필 |
| 본문 (저녁하늘 그라데이션 + 별) | 흰 구름 카드들이 떠 있는 형태 — 글, 미디어 미리보기, 시점 복원 화면 |
| 로그인/회원가입 | 마스코트가 떠다니는(`animate-float`) 풀스크린 별밤 |
| 개발일지 | 피드 글을 근거로 선택, 네이버 글/GitHub 기록/개발 환경 메모를 더해 공부형 또는 당일 개발 경험 본문을 생성하고 발행 |

---

## 시작하기

### 1. 시스템 의존성 (Ubuntu 24.04, 한 번만)

```bash
sudo apt-get install -y \
  libdrogon-dev libjsoncpp-dev uuid-dev libcpp-jwt-dev \
  libssl-dev zlib1g-dev libbrotli-dev libhiredis-dev \
  libpq-dev libmariadb-dev libsqlite3-dev libyaml-cpp-dev \
  libopencv-dev ffmpeg curl \
  docker.io docker-compose-v2
sudo usermod -aG docker $USER  # 로그아웃/재로그인 필요
```

> 호스트에 mariadb-server가 깔려 있으면 3306 충돌 →
> `sudo systemctl stop mariadb && sudo systemctl disable mariadb`

### 2. vcpkg (redis-plus-plus용, 한 번만)

```bash
git clone --depth 1 https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
# CMakeLists가 ~/vcpkg 또는 $VCPKG_ROOT를 자동 감지함.
```

### 3. JWT 개발 키 (한 번만)

```bash
./scripts/gen_dev_jwt_keys.sh
# keys/dev_jwt_{private,public}.pem 생성. .gitignore 처리됨.
```

### 4. 인프라 기동

```bash
docker compose up -d
docker compose ps           # mariadb / redis / minio / ai-hub
```

첫 기동 시 `server/sql/00*.sql`이 자동 실행되어 인증, 글, 미디어, 댓글, 알림, 차단 관련 테이블이 생성됩니다.

### 5. 백엔드 빌드 & 실행

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/mongglemonggle    # → 0.0.0.0:8080
```

첫 빌드는 vcpkg가 redis-plus-plus를 가져오느라 5–10분 더 걸릴 수 있습니다. 이후 빌드는 incremental.

### 6. 프론트엔드 (별도 터미널)

```bash
cd client
npm install               # 첫 1회
npm run dev               # → http://127.0.0.1:5173
```

브라우저에서 `http://127.0.0.1:5173` 접속 → 회원가입 → 로그인 → 글 작성 → 시점 복원 → 사진 첨부 → 친구 팔로우 → 피드.

---

## API 한눈에

### 인증
| Method | Path | 설명 |
|---|---|---|
| POST | `/auth/signup` | 회원가입 → access+refresh |
| POST | `/auth/login` | 로그인 (rate-limited 5/min/IP) |
| POST | `/auth/refresh` | refresh → 새 페어 (회전) |
| POST | `/auth/logout` | refresh 폐기 (204) |
| GET | `/me` | `{user_id, email, display_name, has_avatar}` |

### 글 / 이벤트 소싱
| Method | Path | 설명 |
|---|---|---|
| POST | `/posts` | 글 작성 (10/min/user, 피드 약 1000자, 개발일지 약 20000자) |
| GET / PATCH / DELETE | `/posts/{id}` | 단건 조회 / 수정 / soft delete |
| GET | `/me/timeline` | 본인 글 시간역순 (`category` 필터 지원) |
| GET | `/users/{id}/timeline` | 타인 글 (visibility/follows 필터) |
| GET | `/me/search?q=` | LIKE + 임베딩 혼합 검색 (30/min/user) |
| GET | `/me/snapshot?at=ISO8601` | **임의 시점 상태 복원** |

### 친구 / 피드
| Method | Path | 설명 |
|---|---|---|
| POST / DELETE | `/users/{id}/follow` | 팔로우 / 언팔로우 |
| GET | `/me/followers` `/me/following` | 본인 관계 목록 |
| GET | `/me/feed` | 본인 + 팔로우의 글 (Pull, L1+L2 캐시) |

### 미디어
| Method | Path | 설명 |
|---|---|---|
| POST | `/posts/{id}/media` | multipart/form-data 업로드 (본인 글에만) |
| GET | `/posts/{id}/media` | 첨부 미디어 목록 |
| GET | `/media/{id}/view` | 원본 (visibility 체크) |
| GET | `/media/{id}/download` | attachment 또는 presigned redirect (download_policy 체크) |
| GET | `/media/{id}/thumb` | 썸네일 (사진=jpg, 영상=poster.png) |

### 프로필
| Method | Path | 설명 |
|---|---|---|
| PUT | `/me/avatar` | multipart 업로드 → OpenCV 정사각형 256×256 jpg |
| PATCH | `/me` | 표시 이름 변경 |
| PATCH | `/me/password` | 비밀번호 변경 |
| POST | `/me/verify-password` | 프로필 수정 전 비밀번호 확인 |
| GET | `/users/{id}/avatar` | 공개 — 없으면 프론트에서 첫 글자 표시 |

### 댓글 / 알림 / 차단
| Method | Path | 설명 |
|---|---|---|
| GET / POST | `/posts/{id}/comments` | 댓글 목록 / 작성 |
| DELETE | `/comments/{id}` | 본인 댓글 삭제 |
| GET | `/me/notifications` | 최근 알림 + 미읽음 수 |
| POST | `/me/notifications/read` | 알림 전체 읽음 처리 |
| GET | `/me/notifications/stream` | SSE 알림 스트림 |
| POST / DELETE | `/users/{id}/block` | 차단 / 차단 해제 |
| GET | `/me/blocks` | 차단 목록 |

### 운영
| Method | Path | 설명 |
|---|---|---|
| GET | `/healthz` | 프로세스 헬스 |
| GET | `/readyz` | DB + Redis ping (Redis 끊겨도 L1으로 동작) |
| GET | `/metrics` | Prometheus 텍스트 지표 |
| OPTIONS | `*` | CORS preflight (Vite/Next dev 허용) |

---

## 환경변수

| 이름 | 기본값 |
|---|---|
| `MONGGLE_HTTP_HOST` / `_PORT` | `0.0.0.0` / `8080` |
| `MONGGLE_DB_HOST` / `_PORT` | `127.0.0.1` / `3306` |
| `MONGGLE_DB_NAME` / `_USER` / `_PASSWORD` | `monggle` / `monggle` / `monggle_dev` |
| `MONGGLE_DB_POOL_SIZE` | `8` |
| `MONGGLE_REDIS_HOST` / `_PORT` | `127.0.0.1` / `6379` |
| `MONGGLE_REDIS_POOL_SIZE` | `4` |
| `MONGGLE_JWT_ISSUER` | `monggle.local` |
| `MONGGLE_JWT_PRIVATE_KEY_PATH` | `keys/dev_jwt_private.pem` |
| `MONGGLE_JWT_PUBLIC_KEY_PATH` | `keys/dev_jwt_public.pem` |
| `MONGGLE_JWT_ACCESS_TTL_SECONDS` | `900` (15m) |
| `MONGGLE_JWT_REFRESH_TTL_SECONDS` | `1209600` (14d) |
| `MONGGLE_MEDIA_STORAGE_ROOT` | `media` |
| `MONGGLE_AI_HUB_BASE_URL` | `http://127.0.0.1:9100` |
| `MONGGLE_AI_HUB_TIMEOUT_MS` | `5000` |
| `MONGGLE_S3_ENDPOINT` | `http://127.0.0.1:9002` |
| `MONGGLE_S3_BUCKET` | `monggle-media` |
| `MONGGLE_S3_ACCESS_KEY` / `_SECRET_KEY` | `monggle_admin` / `monggle_dev_secret` |
| `MONGGLE_S3_REGION` | `us-east-1` |
| `MONGGLE_EMBEDDING_MODEL` | `BAAI/bge-m3` |
| `MONGGLE_AI_STUB` | 비어 있음. `1`이면 AI Hub에서 stub 강제 |

---

## 기획서 구현 매핑

| 기획서 축 | 코드 구현 |
|---|---|
| 시점 복원 | `post_events` 이벤트 소싱, `snapshots` 테이블, `SnapshotWorker`, `/me/snapshot` |
| 의미 검색 | AI Hub 임베딩, `embeddings` 테이블, LIKE + cosine similarity 혼합 검색 |
| 자원 설계 | L1 `TtlCache`, L2 `RedisCache`, `/readyz`, `/metrics`, Redis 장애 대응 |
| 트래픽 처리 | Pull 기반 피드, cursor pagination, 피드 캐시, rate limiting |
| 미디어 권한 | visibility와 `download_policy` 분리, view/download 권한 매트릭스 |
| 커뮤니티 흐름 | follow/feed, comments, notifications, SSE, blocks |
| 개발일지 작성 보조 | 피드/네이버/GitHub 근거 import, AI Hub 본문 생성, devlog category 분리 |
| 운영 구성 | Docker Compose 로컬 인프라, AWS Terraform 구성, Prometheus 지표 |

---

## License

학습/포트폴리오 목적의 비공개 작업.
