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
| **로컬 인프라** | Docker Compose (mariadb + redis) | |
| **운영 인프라(예정)** | AWS RDS + ElastiCache + S3 + CloudFront | 기획 14장 |
| **AI 허브(보류)** | Python + BGE-m3 + LLM | MVP 검색은 LIKE |

---

## 디렉터리

```
.
├── doc/                  ← 기획서 PDF, 마스코트, 다이어그램
├── docs/
│   └── DEVLOG.md         ← 개발 일지
├── docker-compose.yml    ← mariadb + redis 로컬 인프라
├── server/               ← C++ 백엔드
│   ├── include/monggle/
│   │   ├── auth/         JwtService, PasswordService, AuthService
│   │   ├── follows/      FollowsService
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
│       └── pages/        Login, Signup, Feed, MyTimeline, Snapshot, Search
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
docker compose ps           # 두 컨테이너 모두 (healthy)
```

첫 기동 시 `server/sql/00*.sql`이 자동 실행되어 10개 테이블이 생성됩니다.

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
| GET | `/users/{id}/avatar` | 공개 — 없으면 404 (프론트는 첫 글자 fallback) |

### 운영
| Method | Path | 설명 |
|---|---|---|
| GET | `/healthz` | 프로세스 헬스 |
| GET | `/readyz` | DB ping (Redis는 후속) |
| OPTIONS | `*` | CORS preflight (Vite/Next dev 허용) |

---

## 의도적으로 보류한 것 (follow-up)

| 항목 | 사유 |
|---|---|
| S3/MinIO 직접 업로드 + 서명 URL | MVP 단축, 로컬 FS로 대체 |
| Redis L2 캐시 + Push fanout | Drogon 1.8.7 RedisClient segfault |
| AI 허브 (BGE-m3 임베딩) | 외부 비용 분리 |
| EventBus 워터마크 | 현재 EventBus 미구현 |
| AWS 인프라 자동화 + 부하 테스트 | 화면 시연 후 |
| 반응형 모바일 사이드바 | 데스크톱 우선 |

자세한 사유는 [docs/DEVLOG.md](docs/DEVLOG.md) 의 "보류" 섹션 참고.

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
