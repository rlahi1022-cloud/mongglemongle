import { useCallback, useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { PostCard } from "@/components/PostCard";
import { EditPostDialog } from "@/components/EditPostDialog";
import { ApiError, posts, type Post, type PostCategory } from "@/api/client";
import { cn } from "@/lib/utils";

const visBadge: Record<string, string> = {
  public: "bg-sky-100 text-sky-700",
  friends: "bg-amber-100 text-amber-800",
  private: "bg-slate-200 text-slate-600",
};
const visLabel: Record<string, string> = {
  public: "전체 공개",
  friends: "친구만",
  private: "나만",
};
const categoryLabel: Record<PostCategory | "all", string> = {
  all: "전체글",
  feed: "피드",
  devlog: "개발일지",
};
const categoryBadge: Record<PostCategory, string> = {
  feed: "bg-emerald-100 text-emerald-700",
  devlog: "bg-violet-100 text-violet-700",
};

export function MyTimelinePage() {
  const [items, setItems] = useState<Post[]>([]);
  const [cursor, setCursor] = useState<number | null>(null);
  const [category, setCategory] = useState<PostCategory | "all">("all");
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [openId, setOpenId] = useState<number | null>(null);
  const [editing, setEditing] = useState<Post | null>(null);

  const load = useCallback(async (cur?: number) => {
    setLoading(true);
    setError(null);
    try {
      const page = await posts.myTimeline(cur, 20, category);
      if (cur) setItems((prev) => [...prev, ...page.items]);
      else     setItems(page.items);
      setCursor(page.next_cursor);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "타임라인 로드 실패");
    } finally { setLoading(false); }
  }, [category]);
  useEffect(() => { load(); }, [load]);

  const onDelete = async (id: number) => {
    if (!confirm("정말 삭제하시겠습니까? 이벤트 이력은 남습니다.")) return;
    try {
      await posts.remove(id);
      setItems((prev) => prev.filter((p) => p.id !== id));
    } catch (e) {
      alert(e instanceof ApiError ? e.message : "실패");
    }
  };

  const onSaved = (updated: Post) => {
    setItems((prev) => prev.map((p) => (p.id === updated.id ? updated : p)));
    setEditing(null);
  };

  return (
    <div className="grid grid-cols-1 gap-5 lg:grid-cols-[180px_minmax(0,1fr)]">
      <aside className="space-y-3 lg:sticky lg:top-24 lg:self-start">
        <div className="cloud-card p-3">
          <div className="px-2 pb-2 text-sm font-bold text-muted-foreground">내 글 보기</div>
          {(["all", "feed", "devlog"] as const).map((value) => (
            <button
              key={value}
              type="button"
              onClick={() => { setOpenId(null); setCategory(value); }}
              className={cn(
                "mb-1 block w-full rounded-2xl px-3 py-2 text-left text-sm font-bold transition",
                category === value ? "bg-primary text-primary-foreground" : "bg-white/80 hover:bg-secondary"
              )}
            >
              {categoryLabel[value]}
            </button>
          ))}
        </div>
      </aside>

      <div className="mx-auto w-full max-w-3xl space-y-5">
        <div className="cloud-card px-5 py-4">
          <h1 className="text-2xl font-bold text-foreground">📓 내 글</h1>
          <p className="text-foreground/70 text-sm mt-1">제목을 클릭하면 본문과 첨부 미디어, 댓글이 펼쳐집니다.</p>
        </div>
        {error && <div className="cloud-card text-sm text-destructive font-medium px-4 py-2">{error}</div>}
        {items.length === 0 && !loading && (
          <Card className="cloud-card">
            <CardContent className="py-12 text-center text-muted-foreground">
              아직 작성한 글이 없습니다.
            </CardContent>
          </Card>
        )}

        <div className="space-y-2">
          {items.map((p) => {
            const open = openId === p.id;
            if (open) {
              return (
                <PostCard
                  key={p.id}
                  postId={p.id}
                  authorId={p.user_id}
                  authorName="나"
                  title={p.title}
                  body={p.body}
                  visibility={p.visibility}
                  createdAt={p.created_at}
                  rightSlot={
                    <>
                      <Button size="sm" variant="ghost" className="rounded-2xl"
                              onClick={(e) => { e.stopPropagation(); setEditing(p); }}>
                        수정
                      </Button>
                      <Button size="sm" variant="ghost" className="rounded-2xl"
                              onClick={(e) => { e.stopPropagation(); setOpenId(null); }}>
                        접기
                      </Button>
                      <Button size="sm" variant="ghost" className="rounded-2xl text-destructive"
                              onClick={(e) => { e.stopPropagation(); onDelete(p.id); }}>
                        삭제
                      </Button>
                    </>
                  }
                />
              );
            }
            return (
              <button
                key={p.id}
                onClick={() => setOpenId(p.id)}
                className={cn(
                  "w-full text-left cloud-card px-4 py-3 flex items-center gap-3",
                  "hover:shadow-md transition-shadow"
                )}
              >
                <div className="flex-1 min-w-0">
                  <div className="font-semibold truncate">
                    {p.title || <span className="text-muted-foreground italic font-normal">제목 없음</span>}
                  </div>
                  <div className="text-xs text-muted-foreground mt-0.5 truncate">
                    {p.body.slice(0, 60)}{p.body.length > 60 ? "..." : ""}
                  </div>
                </div>
                <div className="shrink-0 flex items-center gap-2 text-xs">
                  <span className={`px-2 py-0.5 rounded-full ${categoryBadge[p.category]}`}>
                    {categoryLabel[p.category]}
                  </span>
                  <span className={`px-2 py-0.5 rounded-full ${visBadge[p.visibility] ?? "bg-slate-100"}`}>
                    {visLabel[p.visibility] ?? p.visibility}
                  </span>
                  <span className="text-muted-foreground hidden sm:inline">{p.created_at.split(" ")[0]}</span>
                </div>
              </button>
            );
          })}
        </div>

        {cursor && (
          <div className="text-center">
            <Button variant="outline" className="rounded-2xl bg-white/80 backdrop-blur"
                    onClick={() => load(cursor)} disabled={loading}>
              {loading ? "..." : "더 불러오기"}
            </Button>
          </div>
        )}

        {editing && (
          <EditPostDialog
            initial={editing}
            onClose={() => setEditing(null)}
            onSaved={onSaved}
          />
        )}
      </div>
    </div>
  );
}
