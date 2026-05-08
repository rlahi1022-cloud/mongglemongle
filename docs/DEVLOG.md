# 몽글몽글 구현 노트

> 기획서([docs/몽글몽글_기획.pdf](몽글몽글_기획.pdf))의 핵심 설계를 어떤 코드 구조로 옮겼는지 정리한 문서.

---

## HTTP 서버와 계층 구조

백엔드는 C++ Drogon 기반 REST 서버로 구성했다. 라우트는 `server/src/router/*_routes.cpp`에 모으고, 실제 비즈니스 규칙은 `AuthService`, `PostsService`, `SnapshotService`, `MediaService` 같은 서비스 계층으로 분리했다.

- 라우팅: `drogon::app().registerHandler(...)`, `registerHandlerViaRegex(...)`
- 설정: `AppConfig`가 DB, Redis, JWT, AI Hub, 미디어 저장소 환경변수를 로드
- 응답: JSON API와 `problem+json` 스타일 에러 응답 사용
- 운영 엔드포인트: `/healthz`, `/readyz`, `/metrics`

이 구조 덕분에 인증, 글, 미디어, 알림, 검색 기능이 서로 강하게 얽히지 않고 독립적으로 확장된다.

## 인증과 보안

인증은 JWT RS256 access token과 refresh token 회전 방식으로 구현했다. refresh token은 원문을 저장하지 않고 SHA-256 해시로 관리한다.

- Access token: 15분 TTL
- Refresh token: 14일 TTL
- Refresh 사용 시 새 토큰 페어 발급, 기존 refresh hash 폐기
- 비밀번호: libxcrypt bcrypt `$2b$`
- 로그인, 글 작성, 검색, 미디어 업로드에 rate limit 적용
- CORS preflight와 credential 요청 허용 범위 제어

프로필 수정에는 비밀번호 재확인 게이트(`/me/verify-password`)를 두어 계정 설정 변경 흐름을 한 번 더 보호한다.

## 데이터 모델과 이벤트 소싱

`posts`는 현재 상태를 빠르게 읽기 위한 테이블이고, `post_events`는 글의 변화 이력을 보존하는 이벤트 소스다.

- `created`: 글 생성
- `edited`: 제목 또는 본문 변경
- `visibility_changed`: 공개 범위 변경
- `deleted`: soft delete
- `media_added`: 미디어 첨부
- `restored`: 과거 시점의 삭제 글 복원

글 생성, 수정, 삭제, 복원은 트랜잭션으로 현재 상태 변경과 이벤트 적재를 함께 처리한다. 이 방식으로 현재 타임라인 조회와 과거 시점 복원을 동시에 만족한다.

## 시점 복원

시점 복원은 기획서의 `restore_state(user_id, target_time)` 흐름을 그대로 코드로 옮겼다.

```text
1. target_time 이전 가장 가까운 snapshot 조회
2. snapshot의 event_cursor 이후 이벤트 조회
3. 이벤트를 시간순으로 재생해 해당 시점의 글 상태 계산
```

구현 위치:

- `SnapshotService`: `/me/snapshot?at=...` 요청 처리
- `SnapshotWorker`: 활성 사용자의 현재 상태를 주기적으로 `snapshots`에 저장
- `SnapshotPage`: 날짜/시간 선택 UI와 삭제 글 복원 액션

복원 결과에는 그 시점의 제목, 본문, 공개 범위, 삭제 여부, 마지막 이벤트 ID가 포함된다. 삭제된 글은 “복원” 액션으로 현재 상태에 다시 살릴 수 있고, 이 동작도 `restored` 이벤트로 누적된다.

## 검색과 AI Hub

AI Hub는 Python FastAPI 서비스로 분리했다. 메인 서버는 글 생성/수정 시 본문을 임베딩하고, 검색 시 query 임베딩과 저장된 글 임베딩의 cosine similarity를 계산한다.

- AI Hub: `/embed`, `/compare`, `/devlog/draft`
- DB: `embeddings` 테이블에 post별 vector JSON 저장
- 검색: 임베딩 유사도 결과와 LIKE 키워드 결과를 함께 반환
- 프론트: 키워드 결과와 의미 검색 결과를 구분하고, 결과가 노출된 이유를 표시

AI Hub는 모델 로드 상태에 따라 실제 임베딩 또는 결정적 대체 임베딩을 반환해 API 계약을 유지한다.

## 피드, 팔로우, 캐시

피드는 Pull on read 전략으로 구현했다. 조회 시점에 본인 글, 팔로우한 사용자의 public/friends 글, public 글을 권한 조건에 맞게 합친다.

- 팔로우: `/users/{id}/follow`
- 관계 목록: `/me/followers`, `/me/following`
- 피드: `/me/feed`
- 캐시 key: `feed:{userId}:{cursor}:{limit}`

캐시는 L1/L2 구조다.

- L1: 프로세스 메모리 `TtlCache`
- L2: Redis 기반 `RedisCache`
- `LayeredCache`: L1 hit, L2 hit, miss를 통합 처리
- `/metrics`: cache hit/miss와 Redis 상태 지표 노출

Redis 연결 상태가 흔들려도 L1 캐시와 DB 경로로 서비스가 이어진다.

## 미디어 처리와 권한

미디어는 글에 종속된 자산으로 저장한다. 업로드된 파일은 이미지/영상 종류에 따라 후처리한다.

- 이미지: OpenCV로 200px, 800px JPEG 생성
- 영상: ffmpeg로 첫 프레임 poster PNG 생성
- 프로필 아바타: 정사각형 중앙 crop 후 256x256 JPEG 저장
- 저장소: MinIO/S3 호환 저장소 또는 로컬 FS 경로

조회 권한과 다운로드 권한은 별도 매트릭스로 분리했다.

| 구분 | 역할 |
|---|---|
| `visibility` | 글과 미디어를 볼 수 있는 대상 결정 |
| `download_policy` | 원본 파일 다운로드 가능 대상 결정 |

이 분리 덕분에 “친구에게 보이지만 다운로드는 작성자만 가능” 같은 정책을 표현할 수 있다.

## 댓글, 알림, 차단

커뮤니티 흐름은 댓글, 알림, 차단을 함께 묶어 구현했다.

- 댓글: 조회 권한이 있는 글에만 작성 가능
- 알림: 댓글/팔로우 이벤트 발생 시 DB 적재
- SSE: `/me/notifications/stream`으로 새 알림 실시간 전달
- 차단: 피드, 팔로우, 댓글 권한 판단에 반영

차단 관계는 양방향으로 피드 노출을 막고, 팔로우 생성도 차단한다.

## 개발일지 작성 보조

개발일지는 일반 피드와 별도 카테고리(`devlog`)로 저장한다. 피드 글, 네이버 카페 원문 Markdown, GitHub 커밋 기록을 근거로 모아 AI Hub에 전달하고, 사용자가 선택한 형식에 맞춰 본문을 생성한다.

- 공부형: 개념, 배운 점, 새롭게 느낀 점, 다음 확인 항목
- 당일 개발 경험: 작업 환경, 막힘, 배운 점, 다음 작업
- 네이버 Markdown import
- GitHub commits API import
- 긴 본문 저장을 위한 devlog 전용 길이 허용
- 개발일지 수정 시 일반 피드 수정창이 아닌 큰 Markdown 편집창 사용

근거 자료는 본문에 그대로 붙이지 않고, 사용자가 직접 남긴 메모와 판단을 중심으로 본문을 구성한다.

## 프론트엔드

클라이언트는 React, Vite, TypeScript, Tailwind 기반으로 구성했다.

- `src/api/client.ts`: access token 자동 첨부, 401 시 refresh 재시도
- `AuthContext`: 로그인 상태와 사용자 정보 관리
- `ProtectedRoute`: 인증된 사용자만 주요 페이지 접근
- `PostCard`: 글, 미디어, 댓글 표시 공통 컴포넌트
- `DevlogDraftDialog`: 개발일지 작성 보조
- `EditPostDialog`: 피드/개발일지 카테고리별 편집 UX 분기

주요 화면은 피드, 내 글, 시점 복원, 검색, 프로필, 개발일지 작성 흐름으로 구성된다.

## 운영 구성

로컬 개발 환경은 Docker Compose로 MariaDB, Redis, MinIO, AI Hub를 띄운다. C++ 서버와 React 클라이언트는 로컬 프로세스로 실행한다.

```bash
docker compose up -d
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/mongglemonggle
```

별도 터미널:

```bash
cd client
npm run dev
```

운영 구성은 Terraform으로 VPC, RDS MariaDB, ElastiCache Redis, S3, CloudFront, ALB 리소스를 정의했다.

## 시연 흐름

포트폴리오 시연은 다음 순서로 프로젝트의 핵심 설계를 보여준다.

1. 회원가입과 로그인으로 JWT 흐름 확인
2. 피드 글 작성과 미디어 첨부
3. 팔로우 후 피드 노출 확인
4. 댓글 작성과 알림 확인
5. 검색에서 키워드/의미 결과 비교
6. 시점 복원으로 과거 상태 조회와 삭제 글 복원
7. 피드/네이버/GitHub 근거를 활용한 개발일지 생성
8. `/readyz`, `/metrics`로 운영 지표 확인
