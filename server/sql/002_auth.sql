-- C1: 인증 — refresh token 저장. 회전(rotation) 정책으로 logout/refresh 시 삭제.

CREATE TABLE IF NOT EXISTS refresh_tokens (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id     BIGINT NOT NULL,
    token_hash  CHAR(64) NOT NULL,            -- SHA-256(refresh_token), 길이 64
    issued_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    expires_at  DATETIME(3) NOT NULL,
    revoked     TINYINT(1) NOT NULL DEFAULT 0,
    UNIQUE KEY uniq_token_hash (token_hash),
    INDEX idx_user_expires (user_id, expires_at),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
