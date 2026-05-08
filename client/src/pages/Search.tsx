import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { PostCard } from "@/components/PostCard";
import { ApiError, posts, type Post } from "@/api/client";

function normalize(value: string) {
  return value.toLowerCase().trim();
}

function searchKind(post: Post, query: string) {
  const q = normalize(query);
  const haystack = normalize(`${post.title}\n${post.body}`);
  return haystack.includes(q) ? "keyword" : "semantic";
}

function searchReason(post: Post, query: string) {
  if (searchKind(post, query) === "keyword") {
    return "검색어가 제목이나 본문에 직접 포함되어 있습니다.";
  }
  const body = post.body.replace(/\s+/g, " ").trim();
  const hint = body.length > 70 ? `${body.slice(0, 70)}...` : body;
  return hint
    ? `검색어와 의미가 가까운 기록으로 함께 찾았습니다. 근거 문장: ${hint}`
    : "검색어와 의미가 가까운 기록으로 함께 찾았습니다.";
}

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
    <div className="space-y-5 max-w-3xl mx-auto">
      <div className="cloud-card px-5 py-4">
        <h1 className="text-2xl font-bold text-foreground">🔍 내 글 검색</h1>
        <p className="text-foreground/70 text-sm mt-1">
          키워드가 직접 포함된 글과 AI Hub 임베딩으로 의미가 가까운 글을 함께 찾습니다.
        </p>
      </div>
      <form onSubmit={onSubmit} className="flex gap-2">
        <Input placeholder="키워드..." value={q} onChange={(e) => setQ(e.target.value)} className="rounded-2xl bg-white shadow-md" />
        <Button type="submit" disabled={loading || !q.trim()} className="rounded-2xl">검색</Button>
      </form>
      {error && <div className="cloud-card text-sm text-destructive font-medium px-4 py-2">{error}</div>}
      {searched && items.length === 0 && !loading && (
        <Card className="cloud-card">
          <CardContent className="py-12 text-center text-muted-foreground">
            매칭되는 글이 없습니다.
          </CardContent>
        </Card>
      )}
      {items.map((p) => {
        const kind = searchKind(p, q);
        return (
          <div key={p.id} className="space-y-2">
            <div className="cloud-card flex flex-wrap items-center gap-2 px-4 py-2 text-xs">
              <span className={`rounded-full px-2 py-0.5 font-bold ${
                kind === "keyword" ? "bg-sky-100 text-sky-700" : "bg-violet-100 text-violet-700"
              }`}>
                {kind === "keyword" ? "키워드 검색 결과" : "의미 검색 결과"}
              </span>
              <span className="text-muted-foreground">{searchReason(p, q)}</span>
            </div>
            <PostCard
              postId={p.id}
              authorId={p.user_id}
              authorName="나"
              title={p.title}
              body={p.body}
              visibility={p.visibility}
              createdAt={p.created_at}
            />
          </div>
        );
      })}
    </div>
  );
}
