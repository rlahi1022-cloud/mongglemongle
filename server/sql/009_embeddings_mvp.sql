-- AI Hub 임베딩 검색 MVP 보강.
-- 기존 개발 DB가 001_init.sql 이전 상태여도 필요한 컬럼/테이블을 맞춘다.

SET NAMES utf8mb4;

ALTER TABLE posts
  ADD COLUMN IF NOT EXISTS embedding_id BIGINT NULL AFTER deleted_at;

CREATE TABLE IF NOT EXISTS embeddings (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    post_id         BIGINT NOT NULL,
    user_id         BIGINT NOT NULL,
    model_version   VARCHAR(100) NOT NULL,
    vector_json     JSON NULL,
    created_at      DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    INDEX idx_user (user_id),
    INDEX idx_post (post_id),
    FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE embeddings
  MODIFY COLUMN model_version VARCHAR(100) NOT NULL;
