# 몽글몽글 (Monggle Monggle)

지나간 기록을 꺼내 오늘의 글로 잇는 시스템.
설계 문서: [doc/몽글몽글_기획.pdf](doc/몽글몽글_기획.pdf)

## 목표
- 시점 기반 기록 저장
- 이벤트 소싱 + 스냅샷 기반 시점 복원
- 의미 기반 검색과 임베딩 (AI 허브, 후속)
- 미디어 업로드/백그라운드 처리
- 친구 공유 및 다운로드 워크플로우

## 기술 스택 (확정)
- **백엔드**: C++17, Drogon 1.8 (HTTP), MariaDB 11, Redis 7
- **인증**: JWT RS256 + libbcrypt (예정)
- **클라이언트**: React + Vite (예정)
- **로컬 인프라**: Docker Compose (mariadb + redis)
- **운영 인프라**: AWS (RDS Multi-AZ + ElastiCache + S3 + CloudFront)
- **AI 허브**: Python, BGE-m3 임베딩 — MVP 보류, 검색은 MariaDB FULLTEXT

## 디렉토리 구조
```
server/
  include/monggle/{auth,entry,event,router}/   # 도메인별 헤더
  src/                                         # 도메인별 구현
  sql/                                         # 마이그레이션 / 초기 스키마
client/                                        # React 앱 (예정)
tests/                                         # 단위·통합 테스트 (예정)
doc/                                           # 기획 문서, 다이어그램
docker-compose.yml                             # 로컬 인프라 정의
.env.example                                   # 환경변수 샘플
CMakeLists.txt
```

## 개발 환경 셋업

### 1. 시스템 의존성 (Ubuntu 24.04 기준, 한 번만)
```bash
sudo apt-get install -y \
  libdrogon-dev libjsoncpp-dev uuid-dev \
  libssl-dev zlib1g-dev libbrotli-dev libhiredis-dev \
  libpq-dev libmariadb-dev libsqlite3-dev libyaml-cpp-dev \
  docker.io docker-compose-v2
sudo usermod -aG docker $USER   # 로그아웃/재로그인 필요
```

> **주의**: 호스트에 별도 mariadb-server가 있으면 3306 포트 충돌이 납니다.
> ```bash
> sudo systemctl stop mariadb
> sudo systemctl disable mariadb   # 부팅 시 자동시작 끔
> ```

### 2. 인프라 기동 (mariadb + redis)
```bash
docker compose up -d
docker compose ps     # 두 컨테이너 모두 (healthy) 인지 확인
```
- 첫 기동 시 `server/sql/001_init.sql`이 자동 실행되어 9개 테이블 생성
- 데이터는 docker volume(`monggle_mariadb_data`, `monggle_redis_data`)에 영속

### 3. 빌드
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 4. 실행
```bash
cp .env.example .env   # 필요 시 값 수정
set -a; source .env; set +a
./build/mongglemonggle
```

## 현재 엔드포인트
| Method | Path | 설명 |
|---|---|---|
| GET  | `/healthz` | 프로세스 헬스체크 |
| GET  | `/readyz`  | DB ping (Redis ping은 후속) |
| POST | `/api/entry/create` | 임시 엔트리 (Drogon+서비스 통합 검증용) |

## 다음 구현 단계
1. ~~HTTP 서버 엔진 (`Drogon`)~~ ✅
2. ~~데이터 모델 스키마 + MariaDB Docker 통합~~ ✅
3. JWT(RS256) 인증 + libbcrypt
4. posts CRUD + post_events 이벤트 소싱
5. 시점 복원 알고리즘 (`/me/snapshot?at=...`)
6. 권한 모델 (조회/다운로드 매트릭스)
7. S3 직접 업로드 + 썸네일 워커
8. Redis 캐시 (L1 LRU + L2 Redis), `/readyz`에 Redis ping 추가
9. 팬아웃 Push/Pull Hybrid + Rate Limiting
10. AI 허브 (BGE-m3) — MVP 종료 후
11. AWS 인프라 + 부하 테스트 (k6/wrk)

## 환경변수
| 이름 | 기본값 | 설명 |
|---|---|---|
| `MONGGLE_HTTP_HOST` | `0.0.0.0` | HTTP 서버 바인드 주소 |
| `MONGGLE_HTTP_PORT` | `8080` | HTTP 서버 포트 |
| `MONGGLE_DB_HOST` | `127.0.0.1` | MariaDB 호스트 |
| `MONGGLE_DB_PORT` | `3306` | MariaDB 포트 |
| `MONGGLE_DB_NAME` | `monggle` | DB 스키마명 |
| `MONGGLE_DB_USER` | `monggle` | DB 사용자 |
| `MONGGLE_DB_PASSWORD` | `monggle_dev` | DB 비밀번호 |
| `MONGGLE_DB_POOL_SIZE` | `8` | DB 커넥션 풀 크기 |
| `MONGGLE_REDIS_HOST` | `127.0.0.1` | Redis 호스트 |
| `MONGGLE_REDIS_PORT` | `6379` | Redis 포트 |
| `MONGGLE_REDIS_POOL_SIZE` | `4` | Redis 커넥션 풀 크기 |
