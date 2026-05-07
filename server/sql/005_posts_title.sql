-- 게시글에 제목 필드 추가. 본문(body)과 분리해서 목록·검색 가독성 향상.
-- nullable로 두어 기존 글에 무영향. 프론트는 빈 제목이면 미표시.
ALTER TABLE posts ADD COLUMN title VARCHAR(200) NULL AFTER user_id;
