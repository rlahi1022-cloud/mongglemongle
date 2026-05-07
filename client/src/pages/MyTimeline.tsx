import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { PostCard } from "@/components/PostCard";
import { ApiError, posts, type Post } from "@/api/client";
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

export function MyTimelinePage() {
  const [items, setItems] = useState<Post[]>([]);
  const [cursor, setCursor] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [openId, setOpenId] = useState<number | null>(null);

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
    <div className="space-y-5 max-w-3xl mx-auto">
      <div className="cloud-card px-5 py-4">
        <h1 className="text-2xl font-bold text-foreground">📓 내 글</h1>
        <p className="text-foreground/70 text-sm mt-1">제목을 클릭하면 본문과 첨부 미디어가 펼쳐집니다.</p>
      </div>
      {error && <div className="cloud-card text-sm text-destructive font-medium px-4 py-2">{error}</div>}
      {items.length === 0 && !loading && (
        <Card className="cloud-card">
          <CardContent className="py-12 text-center text-muted-foreground">
            아직 작성한 글이 없습니다.
          </CardContent>
        </Card>
      )}

      {/* 컴팩트 리스트 (제목 + 메타) → 클릭 시 풀 카드로 펼침 */}
      <div className="space-y-2">
        {items.map((p) => {
          const open = openId === p.id;
          if (open) {
            return (
              <div key={p.id}>
                <PostCard
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
              </div>
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
    </div>
  );
}
