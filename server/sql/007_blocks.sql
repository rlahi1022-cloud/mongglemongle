-- 블랙리스트(차단). blocker가 blocked의 글/댓글을 안 보고, 서로 follow 불가.
CREATE TABLE IF NOT EXISTS blocks (
    blocker_id  BIGINT NOT NULL,
    blocked_id  BIGINT NOT NULL,
    created_at  DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (blocker_id, blocked_id),
    INDEX idx_blocked (blocked_id),
    FOREIGN KEY (blocker_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (blocked_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
