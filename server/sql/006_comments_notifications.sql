-- 댓글 + 알림 도입.
CREATE TABLE IF NOT EXISTS comments (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    post_id     BIGINT NOT NULL,
    user_id     BIGINT NOT NULL,
    body        TEXT NOT NULL,
    created_at  DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    deleted_at  DATETIME(3) NULL,
    INDEX idx_post_created (post_id, created_at),
    INDEX idx_user_created (user_id, created_at),
    FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS notifications (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id     BIGINT NOT NULL,            -- 수신자
    kind        VARCHAR(40) NOT NULL,       -- 'follow' | 'comment'
    actor_id    BIGINT NULL,                -- 알림 발생시킨 사용자
    target_id   BIGINT NULL,                -- 관련 post 또는 comment id
    body        TEXT NULL,                  -- 미리보기 텍스트 (예: 댓글 본문 발췌)
    read_at     DATETIME(3) NULL,
    created_at  DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    INDEX idx_user_created (user_id, created_at),
    INDEX idx_user_unread (user_id, read_at),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
