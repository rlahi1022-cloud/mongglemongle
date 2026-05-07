# 몽글몽글 (Monggle Monggle)

<p align="center">
  <img src="doc/마스코트.png" alt="몽글몽글 마스코트" width="180" />
</p>

<p align="center">
  <em>지나간 기록을 꺼내 오늘의 글로 잇는 시스템</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/backend-C++17-blue?style=flat-square" />
  <img src="https://img.shields.io/badge/http-Drogon%201.8-orange?style=flat-square" />
  <img src="https://img.shields.io/badge/db-MariaDB%2011-003545?style=flat-square" />
  <img src="https://img.shields.io/badge/frontend-React%20%2B%20Vite-61dafb?style=flat-square" />
  <img src="https://img.shields.io/badge/style-Tailwind%20%2B%20shadcn-38bdf8?style=flat-square" />
</p>

---

## 무엇을 만들고 있나

매일 무언가를 만드는 사람(개발자/디자이너/학습자)이 자기 글·사진·영상을 **시점과 함께** 저장해 두고, 나중에 임의 시점으로 돌아가 다시 꺼내 볼 수 있게 하는 시스템.

핵심 어필 축 3가지 (기획 1.3):
- **시점 복원** — 이벤트 소싱 + 주기적 스냅샷으로 임의 시점 사용자 상태를 재구성.
- **자원 설계** — 버퍼·큐·메모리 워터마크·백프레셔를 명시적으로 설계.
- **트래픽 처리** — 다중 사용자 환경의 팬아웃·캐시 다층·미디어 다운로드 부하를 다룸.

→ 자세한 설계: [doc/몽글몽글_기획.pdf](doc/몽글몽글_기획.pdf) (40p)
→ 어떻게 코드로 옮겼는지: [docs/DEVLOG.md](docs/DEVLOG.md)

---

## 기술 스택

| 레이어 | 선택 | 비고 |
|---|---|---|
| **백엔드** | C++17, Drogon 1.8 | apt `libdrogon-dev` |
| **인증** | JWT RS256 (cpp-jwt) + bcrypt (libxcrypt) | 토큰 회전, refresh hash 저장 |
| **DB** | MariaDB 11 | Drogon ORM execSqlSync + 트랜잭션 |
| **미디어** | OpenCV 4.6 (썸네일) + ffmpeg (영상 첫프레임) | MVP는 로컬 파일시스템 |
| **클라이언트** | React 18 + Vite 5 + Tailwind 3 + shadcn/ui (TS) | |
| **로컬 인프라** | Docker Compose (MariaDB + Redis + MinIO + AI Hub) | 백엔드는 아직 DB 중심 |
| **운영 인프라(예정)** | AWS RDS + ElastiCache + S3 + CloudFront | Terraform 스켈레톤 |
| **AI 허브** | Python FastAPI stub | MVP 검색은 LIKE |

---

## 디렉터리

```
.
├── doc/                  ← 기획서 PDF, 마스코트, 다이어그램
├── docs/
│   └── DEVLOG.md         ← 개발 일지
├── docker-compose.yml    ← MariaDB, Redis, MinIO, AI Hub 로컬 인프라
├── server/               ← C++ 백엔드
│   ├── include/monggle/
│   │   ├── auth/         JwtService, PasswordService, AuthService
│   │   ├── follows/      FollowsService
│   │   ├── blocks/       BlocksService
│   │   ├── comments/     CommentsService
│   │   ├── notifications/NotificationsService
│   │   ├── media/        MediaService (사진/영상)
│   │   ├── middleware/   cors, rate_limiter, request_log
│   │   ├── posts/        PostsService, SnapshotService
│   │   ├── profile/      ProfileService (아바타)
│   │   └── router/       routes.h
│   ├── src/              위 헤더의 구현
│   └── sql/              초기 스키마 + 마이그레이션
├── client/               ← React 프론트엔드
│   ├── public/mascot.png
│   └── src/
│       ├── api/          백엔드 fetch 래퍼 (auth 자동, refresh 회전)
│       ├── auth/         AuthContext, ProtectedRoute
│       ├── components/   Layout(사이드바), PostCard, ui/*
│       └── pages/        Login, Signup, Feed, MyTimeline, Snapshot, Search, Profile
├── scripts/gen_dev_jwt_keys.sh
└── CMakeLists.txt
```

---

## 화면 미리보기

| 영역 | 설명 |
|---|---|
| 좌측 사이드바 (흰색) | 마스코트 + 메뉴(피드/내 글/시점 복원/검색) + 친구 박스 + 프로필(아바타 클릭으로 업로드) |
| 본문 (저녁하늘 그라데이션 + 별) | 흰 구름 카드들이 떠 있는 형태. 글, 미디어 미리보기, 시점 복원 슬라이더 등 |
| 로그인/회원가입 | 마스코트가 떠다니는(`animate-float`) 풀스크린 별밤 |
| 개발일지 초안 | 피드 글을 체크하고 네이버 글/GitHub 기록/개발 환경 메모를 근거로 공부형/당일 개발 경험 초안 생성 |

---

## 시작하기

### 1. 시스템 의존성 (Ubuntu 24.04, 한 번만)

```bash
sudo apt-get install -y \
  libdrogon-dev libjsoncpp-dev uuid-dev libcpp-jwt-dev \
  libssl-dev zlib1g-dev libbrotli-dev libhiredis-dev \
  libpq-dev libmariadb-dev libsqlite3-dev libyaml-cpp-dev \
  libopencv-dev ffmpeg \
  docker.io docker-compose-v2
sudo usermod -aG docker $USER  # 로그아웃/재로그인 필요
```

> 호스트에 mariadb-server가 깔려 있으면 3306 충돌 →
> `sudo systemctl stop mariadb && sudo systemctl disable mariadb`

### 2. JWT 개발 키 생성 (한 번만)

```bash
./scripts/gen_dev_jwt_keys.sh
# keys/dev_jwt_{private,public}.pem 생성. .gitignore 처리됨.
```

### 3. 인프라 기동

```bash
docker compose up -d
docker compose ps           # mariadb/redis/minio/ai-hub 상태 확인
```

첫 기동 시 `server/sql/00*.sql`이 자동 실행되어 인증, 글, 미디어, 댓글, 알림, 차단 관련 테이블이 생성됩니다.

### 4. 백엔드 빌드 & 실행

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/mongglemonggle    # → 0.0.0.0:8080
```

### 5. 프론트엔드 (별도 터미널, Node 18+)

```bash
cd client
npm install               # 첫 1회
npm run dev               # → http://127.0.0.1:5173
```

브라우저에서 `http://127.0.0.1:5173` 접속 → 회원가입 → 로그인 → 글 작성 → 시점 복원 → 사진 첨부 → 친구 팔로우 → 피드.

피드에서 글을 체크한 뒤 `개발일지 작성`을 누르면 선택한 피드 글, 사용자가 입력한 네이버 글 근거, GitHub 기록, 실제 개발 환경/작업감 메모를 바탕으로 개발일지 초안을 만들 수 있습니다. 형식은 `공부형`과 `당일 개발 경험` 중 선택하며, 공부형은 개념/배운 점/새롭게 느낀 점을 중심으로 구성합니다. 초안 생성기는 입력된 근거가 없는 성과나 작업을 단정하지 않도록 "과장 방지 체크" 섹션을 함께 생성합니다.

---

## API 한눈에 (총 25개 엔드포인트)

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
| POST | `/posts` | 글 작성 (10/min/user, 본문 ≤1000자) |
| GET / PATCH / DELETE | `/posts/{id}` | 단건 조회 / 수정 / soft delete |
| GET | `/me/timeline` | 본인 글 시간역순 |
| GET | `/users/{id}/timeline` | 타인 글 (visibility/follows 필터) |
| GET | `/me/search?q=` | LIKE 키워드 검색 (30/min/user) |
| GET | `/me/snapshot?at=ISO8601` | **임의 시점 상태 복원** |

### 친구 / 피드
| Method | Path | 설명 |
|---|---|---|
| POST / DELETE | `/users/{id}/follow` | 팔로우 / 언팔로우 |
| GET | `/me/followers` `/me/following` | 본인 관계 목록 |
| GET | `/me/feed` | 본인 + 팔로우의 글 (Pull) |

### 미디어
| Method | Path | 설명 |
|---|---|---|
| POST | `/posts/{id}/media` | multipart/form-data 업로드 (본인 글에만) |
| GET | `/posts/{id}/media` | 첨부 미디어 목록 |
| GET | `/media/{id}/view` | 원본 (visibility 체크) |
| GET | `/media/{id}/download` | attachment (download_policy 체크) |
| GET | `/media/{id}/thumb` | 썸네일 (사진=jpg, 영상=poster.png) |

### 프로필
| Method | Path | 설명 |
|---|---|---|
| PUT | `/me/avatar` | multipart 업로드 → OpenCV 정사각형 256x256 jpg |
| PATCH | `/me` | 표시 이름 변경 |
| PATCH | `/me/password` | 비밀번호 변경 |
| POST | `/me/verify-password` | 프로필 수정 전 비밀번호 확인 |
| GET | `/users/{id}/avatar` | 공개 — 없으면 404 (프론트는 첫 글자 fallback) |

### 댓글 / 알림 / 차단
| Method | Path | 설명 |
|---|---|---|
| GET / POST | `/posts/{id}/comments` | 댓글 목록 / 작성 |
| DELETE | `/comments/{id}` | 본인 댓글 삭제 |
| GET | `/me/notifications` | 최근 알림 + 미읽음 수 |
| POST | `/me/notifications/read` | 알림 전체 읽음 처리 |
| POST / DELETE | `/users/{id}/block` | 차단 / 차단 해제 |
| GET | `/me/blocks` | 차단 목록 |

### 운영
| Method | Path | 설명 |
|---|---|---|
| GET | `/healthz` | 프로세스 헬스 |
| GET | `/readyz` | DB ping (Redis는 후속) |
| OPTIONS | `*` | CORS preflight (Vite/Next dev 허용) |

---

## 현재 보류한 것

| 항목 | 상태 |
|---|---|
| S3/MinIO 직접 업로드 + 서명 URL | Compose에는 MinIO가 있으나 백엔드는 로컬 FS 사용 |
| Redis L2 캐시 + Push fanout | Drogon RedisClient 이슈로 readyz Redis ping도 보류 |
| AI 임베딩 검색 | AI Hub는 stub, 실제 검색은 LIKE |
| 네이버/GitHub 자동 수집 | 현재는 사용자가 근거를 입력하는 방식. API 토큰/수집 범위 확정 후 자동 import 예정 |
| 스냅샷 워커 | 시점 복원은 현재 이벤트를 처음부터 재생 |
| 부하 테스트 / 운영 배포 | Terraform 스켈레톤만 있음 |

---

## 환경변수

| 이름 | 기본값 |
|---|---|
| `MONGGLE_HTTP_HOST` / `_PORT` | `0.0.0.0` / `8080` |
| `MONGGLE_DB_HOST` / `_PORT` | `127.0.0.1` / `3306` |
| `MONGGLE_DB_NAME` / `_USER` / `_PASSWORD` | `monggle` / `monggle` / `monggle_dev` |
| `MONGGLE_DB_POOL_SIZE` | `8` |
| `MONGGLE_JWT_ISSUER` | `monggle.local` |
| `MONGGLE_JWT_PRIVATE_KEY_PATH` | `keys/dev_jwt_private.pem` |
| `MONGGLE_JWT_PUBLIC_KEY_PATH` | `keys/dev_jwt_public.pem` |
| `MONGGLE_JWT_ACCESS_TTL_SECONDS` | `900` (15m) |
| `MONGGLE_JWT_REFRESH_TTL_SECONDS` | `1209600` (14d) |
| `MONGGLE_MEDIA_STORAGE_ROOT` | `media` |

---

## License

(미정 — 학습/포트폴리오 목적의 비공개 작업)
