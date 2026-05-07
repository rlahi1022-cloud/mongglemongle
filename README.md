# 몽글몽글 (Monggle Monggle)

지나간 기록을 꺼내 오늘의 글로 잇는 시스템.
설계 문서: [doc/몽글몽글_기획.pdf](doc/몽글몽글_기획.pdf)

## 기술 스택 (확정)
- **백엔드**: C++17, Drogon 1.8 (HTTP), MariaDB 11, OpenCV 4.6 (썸네일), ffmpeg (포스터)
- **인증**: JWT RS256 (cpp-jwt) + bcrypt (libxcrypt crypt_r)
- **클라이언트**: React + Vite (예정)
- **로컬 인프라**: Docker Compose (mariadb + redis)
- **운영 인프라**: AWS (RDS Multi-AZ + ElastiCache + S3 + CloudFront, 후속)
- **AI 허브**: Python BGE-m3 (보류 — MVP 검색은 LIKE)

## 디렉토리 구조
```
server/
  include/monggle/
    auth/        # JwtService, PasswordService, AuthService
    follows/     # FollowsService
    media/       # MediaService
    middleware/  # cors, rate_limiter
    posts/       # PostsService, SnapshotService
    router/      # routes.h
  src/...        # 위 헤더 구현
  sql/           # 001_init.sql, 002_auth.sql
client/          # React 앱 (다음 단계)
tests/           # 단위·통합 테스트 (예정)
doc/             # 기획 문서
docker-compose.yml
.env.example
scripts/gen_dev_jwt_keys.sh
```

## 개발 환경 셋업

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

> 호스트 mariadb가 있으면 3306 충돌 → `sudo systemctl stop mariadb && sudo systemctl disable mariadb`

### 2. JWT 개발 키 생성 (한 번만)
```bash
./scripts/gen_dev_jwt_keys.sh
# keys/dev_jwt_{private,public}.pem 생성. .gitignore 처리됨.
```

### 3. 인프라 기동
```bash
docker compose up -d
docker compose ps  # 두 컨테이너 모두 (healthy) 확인
```
- 첫 기동 시 `server/sql/*.sql`이 자동 실행되어 10개 테이블 생성
- 데이터는 docker volume에 영속

### 4. 빌드 & 실행
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cp .env.example .env  # 필요 시 수정
set -a; source .env; set +a
./build/mongglemonggle
```

## API 엔드포인트 (현재)

### 인증
| Method | Path | 설명 |
|---|---|---|
| POST | `/auth/signup` | 회원가입 → access+refresh |
| POST | `/auth/login` | 로그인 → access+refresh (rate-limited 5/min/IP) |
| POST | `/auth/refresh` | refresh → 새 페어 (회전, 구토큰 폐기) |
| POST | `/auth/logout` | refresh 폐기 (204) |
| GET | `/me` | 자기 user_id |

### 글 / 이벤트 소싱
| Method | Path | 설명 |
|---|---|---|
| POST | `/posts` | 글 작성 (rate-limited 10/min/user, post_events 누적) |
| GET | `/posts/{id}` | 단건 조회 (visibility 체크) |
| PATCH | `/posts/{id}` | 본인만, edited/visibility_changed 이벤트 누적 |
| DELETE | `/posts/{id}` | 본인만, soft delete (deleted 이벤트 누적) |
| GET | `/me/timeline?cursor=&limit=` | 본인 글 시간역순 |
| GET | `/users/{id}/timeline` | 타인 글 (visibility/follows 필터) |
| GET | `/me/search?q=&limit=` | LIKE 키워드 검색 (rate-limited 30/min/user) |
| GET | `/me/snapshot?at=ISO8601` | **임의 시점 상태 복원** (이벤트 재생) |

### 친구 / 피드
| Method | Path | 설명 |
|---|---|---|
| POST | `/users/{id}/follow` | 팔로우 (자기-팔로우 400, 중복 409) |
| DELETE | `/users/{id}/follow` | 언팔로우 |
| GET | `/me/followers` | 본인 팔로워 목록 |
| GET | `/me/following` | 본인 팔로잉 목록 |
| GET | `/me/feed?cursor=&limit=` | 본인 + 팔로우한 사람의 글 (Pull) |

### 미디어
| Method | Path | 설명 |
|---|---|---|
| POST | `/posts/{id}/media` | multipart/form-data 업로드 (사진/영상). 본인 글에만. media_added 이벤트 누적 |
| GET | `/media/{id}/view` | 원본 스트리밍 (visibility 체크) |
| GET | `/media/{id}/download` | Content-Disposition: attachment (download_policy 체크) |
| GET | `/media/{id}/thumb` | 썸네일 (사진=jpg, 영상=poster.png) |

### 운영
| Method | Path | 설명 |
|---|---|---|
| GET | `/healthz` | 프로세스 헬스 |
| GET | `/readyz` | DB ping (Redis는 후속) |
| OPTIONS | `*` | CORS preflight (Vite/Next dev 허용) |

## 동작 검증 완료 시나리오
1. signup → login → /me → refresh → logout → 회전된 토큰 거부
2. alice/bob/charlie/daisy로 friends visibility 동적 체크 (follow/unfollow 즉시 반영)
3. post의 created → edited → visibility_changed → deleted 이벤트가 5건 누적, 시점별 복원 정확
4. 50x50 PNG 업로드 → OpenCV가 width/height 추출 → thumb_200.jpg 자동 생성
5. CORS preflight (5173) 204 + 헤더, 비허용 origin은 헤더 없음
6. /auth/login 5회 초과 시 429 + Retry-After

## 의도적으로 보류한 항목 (follow-up)
- **S3/MinIO 직접 업로드 + 서명 URL** (기획 6장) — MVP는 로컬 FS
- **Redis L2 캐시** (기획 12.2) — Drogon 1.8.7 RedisClient segfault로 분리
- **Push fanout** (기획 12.5) — 현재 Pull only
- **AI 허브 + 임베딩 검색** (기획 5장)
- **EventBus 워터마크** (기획 11.3)
- **AWS 인프라 + 부하 테스트** (기획 14, 15)

## 환경변수
| 이름 | 기본값 |
|---|---|
| `MONGGLE_HTTP_HOST` | `0.0.0.0` |
| `MONGGLE_HTTP_PORT` | `8080` |
| `MONGGLE_DB_HOST` | `127.0.0.1` |
| `MONGGLE_DB_PORT` | `3306` |
| `MONGGLE_DB_NAME` | `monggle` |
| `MONGGLE_DB_USER` | `monggle` |
| `MONGGLE_DB_PASSWORD` | `monggle_dev` |
| `MONGGLE_DB_POOL_SIZE` | `8` |
| `MONGGLE_JWT_ISSUER` | `monggle.local` |
| `MONGGLE_JWT_PRIVATE_KEY_PATH` | `keys/dev_jwt_private.pem` |
| `MONGGLE_JWT_PUBLIC_KEY_PATH` | `keys/dev_jwt_public.pem` |
| `MONGGLE_JWT_ACCESS_TTL_SECONDS` | `900` (15m) |
| `MONGGLE_JWT_REFRESH_TTL_SECONDS` | `1209600` (14d) |
| `MONGGLE_MEDIA_STORAGE_ROOT` | `media` |
