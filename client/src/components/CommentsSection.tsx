import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { ApiError, comments, profile, type CommentItem } from "@/api/client";
import { useAuth } from "@/auth/AuthContext";

interface Props {
  postId: number;
}

export function CommentsSection({ postId }: Props) {
  const { userId } = useAuth();
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState<CommentItem[]>([]);
  const [loading, setLoading] = useState(false);
  const [body, setBody] = useState("");
  const [posting, setPosting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const refresh = async () => {
    setLoading(true);
    setError(null);
    try {
      const r = await comments.list(postId);
      setItems(r.items);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "댓글 로드 실패");
    } finally { setLoading(false); }
  };

  useEffect(() => {
    if (open && items.length === 0 && !loading) refresh();
  }, [open]);

  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!body.trim()) return;
    setPosting(true);
    try {
      const created = await comments.create(postId, body.trim());
      setItems((prev) => [...prev, created]);
      setBody("");
    } catch (e) {
      alert(e instanceof ApiError ? e.message : "댓글 작성 실패");
    } finally { setPosting(false); }
  };

  const onDelete = async (id: number) => {
    if (!confirm("댓글을 삭제할까요?")) return;
    try {
      await comments.remove(id);
      setItems((prev) => prev.filter((c) => c.id !== id));
    } catch (e) {
      alert(e instanceof ApiError ? e.message : "실패");
    }
  };

  return (
    <div className="border-t pt-3 mt-2">
      <button
        type="button"
        onClick={() => setOpen((v) => !v)}
        className="text-sm text-muted-foreground hover:text-foreground transition flex items-center gap-1"
      >
        💬 댓글 {open ? "접기" : "보기"} {items.length > 0 && `(${items.length})`}
      </button>
      {open && (
        <div className="space-y-3 mt-3">
          {error && <div className="text-xs text-destructive">{error}</div>}
          {loading && <div className="text-xs text-muted-foreground">불러오는 중...</div>}
          {!loading && items.length === 0 && (
            <div className="text-xs text-muted-foreground">아직 댓글이 없어요.</div>
          )}
          {items.map((c) => (
            <div key={c.id} className="flex items-start gap-2 text-sm">
              <div className="relative h-7 w-7 shrink-0 rounded-full bg-secondary text-foreground grid place-items-center text-xs font-bold overflow-hidden">
                <img
                  src={profile.avatarUrl(c.user_id)}
                  alt=""
                  className="absolute inset-0 w-full h-full object-cover"
                  onError={(e) => { (e.currentTarget as HTMLImageElement).style.display = "none"; }}
                />
                <span className="z-0">{c.author_name?.charAt(0)?.toUpperCase() || "?"}</span>
              </div>
              <div className="flex-1 min-w-0">
                <div className="flex items-center gap-2">
                  <span className="font-medium text-sm">{c.author_name}</span>
                  <span className="text-xs text-muted-foreground">{c.created_at.split(" ")[0]}</span>
                  {userId === c.user_id && (
                    <button
                      onClick={() => onDelete(c.id)}
                      className="text-xs text-destructive/70 hover:text-destructive ml-auto"
                    >
                      삭제
                    </button>
                  )}
                </div>
                <div className="whitespace-pre-wrap text-sm">{c.body}</div>
              </div>
            </div>
          ))}
          <form onSubmit={onSubmit} className="flex gap-2 mt-2">
            <input
              value={body}
              onChange={(e) => setBody(e.target.value)}
              placeholder="댓글 작성..."
              maxLength={2000}
              className="h-9 flex-1 rounded-2xl bg-white border border-border px-3 text-sm focus:outline-none focus:ring-2 focus:ring-ring"
            />
            <Button type="submit" size="sm" disabled={posting || !body.trim()} className="rounded-2xl">
              {posting ? "..." : "쓰기"}
            </Button>
          </form>
        </div>
      )}
    </div>
  );
}
