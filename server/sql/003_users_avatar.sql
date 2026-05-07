-- C-followup: 사용자 프로필 사진 경로 컬럼.
-- 실제 파일은 storage_root 안에 avatars/{user_id}.{ext} 형태로 저장.
ALTER TABLE users ADD COLUMN avatar_path VARCHAR(512) NULL;
