-- 몽글몽글 초기 스키마
-- 기획서 7장(데이터 모델), 9장(미디어 저장) 기반
-- AI 허브 MVP 보류로 embeddings 테이블은 스키마만 존재 (워커 후속)

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- 7.2 users : 사용자 계정
CREATE TABLE IF NOT EXISTS users (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    email           VARCHAR(255) NOT NULL,
    password_hash   VARCHAR(255) NOT NULL,
    display_name    VARCHAR(100) NOT NULL,
    profile_json    JSON         NULL,
    created_at      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at      DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    UNIQUE KEY uniq_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7.6 follows : 단방향 팔로우
CREATE TABLE IF NOT EXISTS follows (
    follower_id     BIGINT NOT NULL,
    followee_id     BIGINT NOT NULL,
    created_at      DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (follower_id, followee_id),
    INDEX idx_followee (followee_id),
    FOREIGN KEY (follower_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (followee_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7.3 posts : 발행 콘텐츠 (현재 상태 캐시)
CREATE TABLE IF NOT EXISTS posts (
    id               BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id          BIGINT NOT NULL,
    body             TEXT NOT NULL,
    visibility       ENUM('public','friends','private') NOT NULL,
    download_policy  ENUM('owner_only','followers','public_allowed') NOT NULL DEFAULT 'owner_only',
    created_at       DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at       DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    deleted_at       DATETIME(3) NULL,                     -- soft delete
    embedding_id     BIGINT NULL,                          -- AI 허브 후속
    INDEX idx_user_created (user_id, created_at),
    INDEX idx_visibility_created (visibility, created_at),
    FULLTEXT KEY ft_body (body),                           -- MVP 키워드 검색
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7.4 post_events : 시점 복원용 이벤트 소스
CREATE TABLE IF NOT EXISTS post_events (
    id           BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id      BIGINT NOT NULL,
    post_id      BIGINT NOT NULL,
    event_type   ENUM('created','edited','deleted','visibility_changed','media_added') NOT NULL,
    payload_json JSON NOT NULL,
    occurred_at  DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    INDEX idx_user_time (user_id, occurred_at),
    INDEX idx_post_time (post_id, occurred_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7.5 snapshots : 주기적 사용자 상태 스냅샷 (시점 복원 가속)
CREATE TABLE IF NOT EXISTS snapshots (
    id            BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id       BIGINT NOT NULL,
    taken_at      DATETIME(3) NOT NULL,
    state_json    LONGBLOB NOT NULL,                       -- gzip 압축된 JSON
    event_cursor  BIGINT NOT NULL,                         -- 마지막 반영 post_events.id
    INDEX idx_user_time (user_id, taken_at),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 9.4 media_assets : 미디어 메타데이터 (S3 키 포함)
CREATE TABLE IF NOT EXISTS media_assets (
    id               BIGINT AUTO_INCREMENT PRIMARY KEY,
    post_id          BIGINT NOT NULL,
    user_id          BIGINT NOT NULL,
    kind             ENUM('photo','video','external_embed') NOT NULL,
    s3_key_original  VARCHAR(512) NULL,
    s3_key_thumb     VARCHAR(512) NULL,
    s3_key_poster    VARCHAR(512) NULL,
    external_url     VARCHAR(1024) NULL,
    mime_type        VARCHAR(100) NULL,
    size_bytes       BIGINT NULL,
    width_px         INT NULL,
    height_px        INT NULL,
    duration_ms      INT NULL,
    status           ENUM('pending','ready','failed') NOT NULL,
    created_at       DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    INDEX idx_post (post_id),
    INDEX idx_user (user_id),
    FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7.2 activity_events : 외부 자동 수집 활동 (Git 등). 본인만, 절대 비공개
CREATE TABLE IF NOT EXISTS activity_events (
    id           BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id      BIGINT NOT NULL,
    source       VARCHAR(50) NOT NULL,
    payload_json JSON NOT NULL,
    occurred_at  DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    INDEX idx_user_time (user_id, occurred_at),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7.2 embeddings : 텍스트 임베딩 인덱스 (MVP 보류, 스키마만)
CREATE TABLE IF NOT EXISTS embeddings (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    post_id         BIGINT NOT NULL,
    user_id         BIGINT NOT NULL,
    model_version   VARCHAR(50) NOT NULL,
    vector_json     JSON NULL,                             -- pgvector 미사용 시 임시
    created_at      DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    INDEX idx_user (user_id),
    INDEX idx_post (post_id),
    FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7.2 archives : 일괄 다운로드 작업 상태
CREATE TABLE IF NOT EXISTS archives (
    id               BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id          BIGINT NOT NULL,
    period           VARCHAR(20) NOT NULL,                 -- e.g. '2026-03'
    status           ENUM('queued','building','ready','failed','expired') NOT NULL,
    s3_key           VARCHAR(512) NULL,
    idempotency_key  VARCHAR(100) NULL,
    requested_at     DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    completed_at     DATETIME(3) NULL,
    expires_at       DATETIME(3) NULL,                     -- lifecycle 7일
    INDEX idx_user_time (user_id, requested_at),
    UNIQUE KEY uniq_user_period_idem (user_id, period, idempotency_key),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

SET FOREIGN_KEY_CHECKS = 1;
