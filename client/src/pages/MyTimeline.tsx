import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { PostCard } from "@/components/PostCard";
import { ApiError, posts, type Post } from "@/api/client";

export function MyTimelinePage() {
  const [items, setItems] = useState<Post[]>([]);
  const [cursor, setCursor] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const load = async (cur?: number) => {
    setLoading(true);
    setError(null);
    try {
      const page = await posts.myTimeline(cur);
      setItems(cur ? (prev) => [...prev, ...page.items] as any : page.items);
      setCursor(page.next_cursor);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "타임라인 로드 실패");
    } finally { setLoading(false); }
  };
  useEffect(() => { load(); }, []);

  const onDelete = async (id: number) => {
    if (!confirm("정말 삭제하시겠습니까? 이벤트 이력은 남습니다.")) return;
    try {
      await posts.remove(id);
      setItems((prev) => prev.filter((p) => p.id !== id));
    } catch (e) {
      alert(e instanceof ApiError ? e.message : "실패");
    }
  };

  return (
    <div className="space-y-4 max-w-2xl mx-auto">
      <h1 className="text-2xl font-bold">내 글</h1>
      {error && <div className="text-sm text-destructive">{error}</div>}
      {items.length === 0 && !loading && (
        <Card><CardContent className="py-10 text-center text-muted-foreground">
          아직 작성한 글이 없습니다.
        </CardContent></Card>
      )}
      {items.map((p) => (
        <PostCard
          key={p.id}
          authorName={`나 (#${p.user_id})`}
          body={p.body}
          visibility={p.visibility}
          createdAt={p.created_at}
          rightSlot={
            <Button size="sm" variant="ghost" onClick={() => onDelete(p.id)}>삭제</Button>
          }
        />
      ))}
      {cursor && (
        <div className="text-center">
          <Button variant="outline" onClick={() => load(cursor)} disabled={loading}>
            {loading ? "..." : "더 불러오기"}
          </Button>
        </div>
      )}
    </div>
  );
}
