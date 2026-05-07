-- 일반 피드 글과 개발일지를 구분하는 카테고리.
ALTER TABLE posts
  ADD COLUMN category ENUM('feed','devlog') NOT NULL DEFAULT 'feed' AFTER title,
  ADD INDEX idx_user_category_created (user_id, category, created_at);
