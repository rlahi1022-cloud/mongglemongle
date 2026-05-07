import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { PostCard } from "@/components/PostCard";
import { ApiError, posts, type Post } from "@/api/client";

export function SearchPage() {
  const [q, setQ] = useState("");
  const [items, setItems] = useState<Post[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [searched, setSearched] = useState(false);

  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!q.trim()) return;
    setLoading(true);
    setError(null);
    setSearched(true);
    try {
      const r = await posts.search(q.trim());
      setItems(r.items);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "검색 실패");
    } finally { setLoading(false); }
  };

  return (
    <div className="space-y-5">
      <h1 className="text-2xl font-bold text-white drop-shadow">🔍 내 글 검색</h1>
      <p className="text-white/80 text-sm -mt-2">
        본인 글에서 키워드를 찾습니다. 의미 검색은 AI 허브 도입 후 예정.
      </p>
      <form onSubmit={onSubmit} className="flex gap-2">
        <Input placeholder="키워드..." value={q} onChange={(e) => setQ(e.target.value)} className="rounded-2xl bg-white" />
        <Button type="submit" disabled={loading || !q.trim()} className="rounded-2xl">검색</Button>
      </form>
      {error && <div className="text-sm text-white bg-destructive/80 rounded-2xl px-4 py-2">{error}</div>}
      {searched && items.length === 0 && !loading && (
        <Card className="cloud-card">
          <CardContent className="py-12 text-center text-muted-foreground">
            매칭되는 글이 없습니다.
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
        />
      ))}
    </div>
  );
}
