-- 시점 복원 화면에서 삭제된 글을 다시 살릴 수 있도록 'restored' 이벤트 추가.
ALTER TABLE post_events
  MODIFY COLUMN event_type
    ENUM('created','edited','deleted','visibility_changed','media_added','restored')
    NOT NULL;
