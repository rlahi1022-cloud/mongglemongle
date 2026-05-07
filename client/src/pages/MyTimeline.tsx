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
      if (cur) {
        setItems((prev) => [...prev, ...page.items]);
      } else {
        setItems(page.items);
      }
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
    <div className="space-y-5">
      <h1 className="text-2xl font-bold text-white drop-shadow">📓 내 글</h1>
      {error && <div className="text-sm text-white bg-destructive/80 rounded-2xl px-4 py-2">{error}</div>}
      {items.length === 0 && !loading && (
        <Card className="cloud-card">
          <CardContent className="py-12 text-center text-muted-foreground">
            아직 작성한 글이 없습니다.
          </CardContent>
        </Card>
      )}
      {items.map((p) => (
        <PostCard
          key={p.id}
          authorName="나"
          body={p.body}
          visibility={p.visibility}
          createdAt={p.created_at}
          rightSlot={
            <Button size="sm" variant="ghost" className="rounded-2xl" onClick={() => onDelete(p.id)}>삭제</Button>
          }
        />
      ))}
      {cursor && (
        <div className="text-center">
          <Button variant="outline" className="rounded-2xl bg-white/80 backdrop-blur"
                  onClick={() => load(cursor)} disabled={loading}>
            {loading ? "..." : "더 불러오기"}
          </Button>
        </div>
      )}
    </div>
  );
}
