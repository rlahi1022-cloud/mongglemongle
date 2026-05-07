import { useCallback, useEffect, useState } from "react";
import { ApiError, posts, social, type FeedItem, type Post } from "@/api/client";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { DevlogDraftDialog } from "@/components/DevlogDraftDialog";
import { PostCard } from "@/components/PostCard";

export function DevlogsPage() {
  const [items, setItems] = useState<FeedItem[]>([]);
  const [devlogs, setDevlogs] = useState<Post[]>([]);
  const [selectedPostIds, setSelectedPostIds] = useState<number[]>([]);
  const [nextCursor, setNextCursor] = useState<number | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [draftOpen, setDraftOpen] = useState(false);

  const selectedPosts = items.filter((item) => selectedPostIds.includes(item.id));

  const refreshEvidence = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const page = await social.feed();
      setItems(page.items);
      setNextCursor(page.next_cursor);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "근거 글 로드 실패");
    } finally {
      setLoading(false);
    }
  }, []);

  const refreshDevlogs = useCallback(async () => {
    try {
      const page = await posts.myTimeline(undefined, 20, "devlog");
      setDevlogs(page.items);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "개발일지 로드 실패");
    }
  }, []);

  const refreshAll = useCallback(async () => {
    await Promise.all([refreshEvidence(), refreshDevlogs()]);
  }, [refreshDevlogs, refreshEvidence]);

  useEffect(() => { refreshAll(); }, [refreshAll]);

  const toggleSelected = (postId: number) => {
    setSelectedPostIds((prev) =>
      prev.includes(postId) ? prev.filter((id) => id !== postId) : [...prev, postId]
    );
  };

  const loadMore = async () => {
    if (!nextCursor) return;
    setLoading(true);
    try {
      const page = await social.feed(nextCursor);
      setItems((prev) => [...prev, ...page.items]);
      setNextCursor(page.next_cursor);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "근거 글 로드 실패");
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="mx-auto max-w-3xl space-y-5">
      <div className="cloud-card flex flex-wrap items-center gap-3 px-4 py-3">
        <div>
          <h1 className="text-lg font-bold">개발일지</h1>
          <div className="text-xs text-muted-foreground">선택한 근거 {selectedPostIds.length}개</div>
        </div>
        <div className="flex-1" />
        <Button
          type="button"
          disabled={selectedPostIds.length === 0}
          onClick={() => setDraftOpen(true)}
          className="rounded-2xl"
        >
          개발일지 작성
        </Button>
      </div>

      {error && <div className="cloud-card px-4 py-2 text-sm font-medium text-destructive">{error}</div>}

      {devlogs.length > 0 && (
        <section className="space-y-3">
          <h2 className="cloud-card px-4 py-3 text-base font-bold">내 개발일지</h2>
          {devlogs.map((item) => (
            <PostCard
              key={item.id}
              postId={item.id}
              authorId={item.user_id}
              authorName="나"
              title={item.title}
              body={item.body}
              visibility={item.visibility}
              createdAt={item.created_at}
            />
          ))}
        </section>
      )}

      {items.length === 0 && !loading && (
        <Card className="cloud-card">
          <CardContent className="py-12 text-center text-muted-foreground">
            선택할 근거 글이 아직 없어요.
          </CardContent>
        </Card>
      )}

      {items.map((item) => (
        <PostCard
          key={item.id}
          postId={item.id}
          authorId={item.user_id}
          authorName={item.author_name}
          title={item.title}
          body={item.body}
          visibility={item.visibility}
          createdAt={item.created_at}
          showComments={false}
          rightSlot={
            <label className="flex h-8 items-center gap-2 rounded-2xl border bg-white/80 px-2 text-xs font-bold">
              <input
                type="checkbox"
                checked={selectedPostIds.includes(item.id)}
                onChange={() => toggleSelected(item.id)}
                className="h-4 w-4 accent-primary"
              />
              선택
            </label>
          }
        />
      ))}

      {nextCursor && (
        <div className="text-center">
          <Button
            type="button"
            variant="outline"
            onClick={loadMore}
            disabled={loading}
            className="rounded-2xl bg-white/80"
          >
            {loading ? "..." : "더 불러오기"}
          </Button>
        </div>
      )}

      {draftOpen && (
        <DevlogDraftDialog
          selectedPosts={selectedPosts}
          onClose={() => setDraftOpen(false)}
          onPublished={refreshAll}
        />
      )}
    </div>
  );
}
