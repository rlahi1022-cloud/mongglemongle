import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { PostCard } from "@/components/PostCard";
import { ApiError, posts, type Post } from "@/api/client";

type ResultFilter = "all" | "keyword" | "semantic";

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

function highlightParts(text: string, query: string) {
  const q = query.trim();
  if (!q) return [text];
  const lower = text.toLowerCase();
  const needle = q.toLowerCase();
  const out: string[] = [];
  let cursor = 0;
  let idx = lower.indexOf(needle);
  while (idx >= 0) {
    if (idx > cursor) out.push(text.slice(cursor, idx));
    out.push(text.slice(idx, idx + q.length));
    cursor = idx + q.length;
    idx = lower.indexOf(needle, cursor);
  }
  if (cursor < text.length) out.push(text.slice(cursor));
  return out.length > 0 ? out : [text];
}

function Highlight({ text, query }: { text: string; query: string }) {
  return (
    <>
      {highlightParts(text, query).map((part, idx) => (
        part.toLowerCase() === query.trim().toLowerCase()
          ? <mark key={`${part}-${idx}`} className="rounded bg-yellow-100 px-0.5 text-foreground">{part}</mark>
          : <span key={`${part}-${idx}`}>{part}</span>
      ))}
    </>
  );
}

function resultSnippet(post: Post) {
  const body = post.body.replace(/\s+/g, " ").trim();
  return body.length > 120 ? `${body.slice(0, 120)}...` : body;
}

export function SearchPage() {
  const [q, setQ] = useState("");
  const [searchedQuery, setSearchedQuery] = useState("");
  const [items, setItems] = useState<Post[]>([]);
  const [filter, setFilter] = useState<ResultFilter>("all");
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [searched, setSearched] = useState(false);

  const counts = items.reduce(
    (acc, post) => {
      const kind = searchKind(post, searchedQuery);
      acc.all += 1;
      acc[kind] += 1;
      return acc;
    },
    { all: 0, keyword: 0, semantic: 0 }
  );
  const visibleItems = items.filter((post) => filter === "all" || searchKind(post, searchedQuery) === filter);

  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!q.trim()) return;
    setLoading(true);
    setError(null);
    setSearched(true);
    setFilter("all");
    try {
      const query = q.trim();
      const r = await posts.search(query);
      setSearchedQuery(query);
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
      {searched && items.length > 0 && (
        <div className="cloud-card flex flex-wrap gap-2 px-3 py-2">
          {([
            ["all", `전체 ${counts.all}`],
            ["keyword", `키워드 ${counts.keyword}`],
            ["semantic", `의미 ${counts.semantic}`],
          ] as const).map(([value, label]) => (
            <button
              key={value}
              type="button"
              onClick={() => setFilter(value)}
              className={`rounded-2xl px-3 py-1.5 text-sm font-bold transition ${
                filter === value ? "bg-primary text-primary-foreground" : "bg-white/80 text-foreground hover:bg-secondary"
              }`}
            >
              {label}
            </button>
          ))}
        </div>
      )}
      {error && <div className="cloud-card text-sm text-destructive font-medium px-4 py-2">{error}</div>}
      {searched && visibleItems.length === 0 && !loading && (
        <Card className="cloud-card">
          <CardContent className="py-12 text-center text-muted-foreground">
            매칭되는 글이 없습니다.
          </CardContent>
        </Card>
      )}
      {visibleItems.map((p) => {
        const kind = searchKind(p, searchedQuery);
        const snippet = resultSnippet(p);
        return (
          <div key={p.id} className="space-y-2">
            <div className="cloud-card space-y-1 px-4 py-2 text-xs">
              <div className="flex flex-wrap items-center gap-2">
              <span className={`rounded-full px-2 py-0.5 font-bold ${
                kind === "keyword" ? "bg-sky-100 text-sky-700" : "bg-violet-100 text-violet-700"
              }`}>
                {kind === "keyword" ? "키워드 검색 결과" : "의미 검색 결과"}
              </span>
                <span className="text-muted-foreground">{searchReason(p, searchedQuery)}</span>
              </div>
              {kind === "keyword" && (
                <div className="text-muted-foreground">
                  <Highlight text={`${p.title ? `${p.title} - ` : ""}${snippet}`} query={searchedQuery} />
                </div>
              )}
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
