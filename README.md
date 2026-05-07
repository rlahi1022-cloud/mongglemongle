# 몽글몽글 (Monggle Monggle)

이 리포지토리는 `몽글몽글` 서비스의 초기 뼈대를 제공합니다.

## 목표
- 시점 기반 기록 저장
- 이벤트 소싱 + 스냅샷 기반 시점 복원
- 의미 기반 검색과 임베딩
- 미디어 업로드/백그라운드 처리
- 친구 공유 및 다운로드 워크플로우

## 현재 구성
- `server/`: 백엔드 서버 구현
  - `server/include/monggle/`: 기능별 헤더
  - `server/src/`: 기능별 소스
- `client/`: 클라이언트 앱 자리 표시자
- `CMakeLists.txt`: 서버 빌드 설정
- `doc/몽글몽글_기획.pdf`: 프로젝트 설계 문서

## 폴더 구조
- `server/auth/`: 인증 관련 서비스
- `server/entry/`: 콘텐츠 작성/이벤트 처리
- `server/event/`: 이벤트 버스 / 이벤트 소싱
- `server/router/`: HTTP 라우팅과 엔드포인트 구성

## 빌드 및 실행

### 의존성 (Ubuntu 24.04)
```bash
sudo apt-get install -y \
  libdrogon-dev libjsoncpp-dev uuid-dev \
  libssl-dev zlib1g-dev libbrotli-dev libhiredis-dev \
  libpq-dev libmariadb-dev libsqlite3-dev libyaml-cpp-dev
```

### 빌드
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 실행
```bash
./build/mongglemonggle
# 0.0.0.0:8080 에서 listen
```

### 현재 엔드포인트
- `GET  /healthz`            — 헬스체크
- `POST /api/entry/create`   — 임시 엔트리 작성 (Drogon 통합 검증용)

## 다음 구현 단계
1. ~~HTTP 서버 엔진 선택 (`Drogon`)~~ ✅
2. 인증/세션 관리 (JWT Access+Refresh, bcrypt)
3. 데이터 모델 DDL + MariaDB 커넥션 풀
4. 이벤트 소싱과 스냅샷 복원 (`post_events`, `snapshots`)
5. 권한 모델 (조회/다운로드 매트릭스)
6. S3 직접 업로드 + 썸네일 워커
7. Redis 캐시 (L1 LRU + L2 Redis)
8. AI 허브 (Python, BGE-m3 임베딩)
9. 팬아웃 (Push/Pull Hybrid) + Rate Limiting
10. AWS 인프라 + 부하 테스트 (k6/wrk)
