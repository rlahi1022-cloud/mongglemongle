import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { VisibilitySelect } from "@/components/ui/select-visibility";
import { PostCard } from "@/components/PostCard";
import { FriendsBox } from "@/components/FriendsBox";
import { DevlogDraftDialog } from "@/components/DevlogDraftDialog";
import {
  ApiError,
  posts,
  social,
  uploadMedia,
  POST_BODY_MAX,
  type FeedItem,
  type Visibility,
} from "@/api/client";

export function FeedPage() {
  const [items, setItems] = useState<FeedItem[]>([]);
  const [nextCursor, setNextCursor] = useState<number | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [composerOpen, setComposerOpen] = useState(false);
  const [devlogOpen, setDevlogOpen] = useState(false);

  const [title, setTitle] = useState("");
  const [body, setBody] = useState("");
  const [visibility, setVisibility] = useState<Visibility>("public");
  const [file, setFile] = useState<File | null>(null);
  const [posting, setPosting] = useState(false);

  const refresh = async () => {
    setLoading(true);
    setError(null);
    try {
      const page = await social.feed();
      setItems(page.items);
      setNextCursor(page.next_cursor);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "피드 로드 실패");
    } finally { setLoading(false); }
  };

  useEffect(() => { refresh(); }, []);

  const onPost = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!body.trim()) return;
    setPosting(true);
    try {
      const created = await posts.create(title.trim(), body.trim(), visibility);
      if (file) {
        try { await uploadMedia(created.id, file); } catch { /* ignore */ }
      }
      setTitle("");
      setBody("");
      setFile(null);
      setComposerOpen(false);
      const inp = document.getElementById("media-input") as HTMLInputElement | null;
      if (inp) inp.value = "";
      await refresh();
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "글 작성 실패");
    } finally { setPosting(false); }
  };

  const loadMore = async () => {
    if (!nextCursor) return;
    setLoading(true);
    try {
      const page = await social.feed(nextCursor);
      setItems((prev) => [...prev, ...page.items]);
      setNextCursor(page.next_cursor);
    } finally { setLoading(false); }
  };

  return (
    <div className="grid grid-cols-1 lg:grid-cols-[1fr_280px] gap-6">
      <div className="space-y-5 min-w-0">
        <div className="cloud-card flex flex-wrap items-center gap-3 px-4 py-3">
          <div>
            <h1 className="text-lg font-bold">☁️ 피드</h1>
            <div className="text-xs text-muted-foreground">기록을 먼저 읽고, 필요할 때 새 글을 남겨요.</div>
          </div>
          <div className="flex-1" />
          <Button
            type="button"
            variant={composerOpen ? "secondary" : "default"}
            onClick={() => setComposerOpen((v) => !v)}
            className="rounded-2xl"
          >
            {composerOpen ? "글쓰기 닫기" : "새 글 쓰기"}
          </Button>
          <Button
            type="button"
            variant="outline"
            onClick={() => setDevlogOpen(true)}
            className="rounded-2xl border-sky-200 bg-sky-50 px-3 text-sm font-bold text-sky-700 hover:bg-sky-100"
          >
            개발일지 작성
          </Button>
        </div>

        {composerOpen && (
          <Card className="cloud-card">
            <CardHeader className="pb-3">
              <CardTitle className="text-base flex items-center gap-2">
                <span>☁️</span> 새 글
              </CardTitle>
            </CardHeader>
            <CardContent>
            <form onSubmit={onPost} className="space-y-3">
              <Input
                placeholder="제목 (선택)"
                value={title}
                onChange={(e) => setTitle(e.target.value.slice(0, 200))}
                maxLength={200}
                className="rounded-2xl font-medium"
              />
              <Textarea
                placeholder="오늘 무엇을 했나요? 시간이 흐른 뒤 다시 꺼내볼 한 줄."
                value={body}
                onChange={(e) => setBody(e.target.value.slice(0, POST_BODY_MAX))}
                rows={4}
                maxLength={POST_BODY_MAX}
                className="rounded-2xl"
              />
              <div className={`text-xs text-right ${body.length > POST_BODY_MAX * 0.9 ? "text-destructive" : "text-muted-foreground"}`}>
                {body.length} / {POST_BODY_MAX}
              </div>
              <div className="flex flex-wrap items-center gap-3">
                <VisibilitySelect value={visibility} onChange={setVisibility} />
                <label
                  htmlFor="media-input"
                  className="inline-flex h-10 cursor-pointer items-center rounded-2xl border border-sky-200 bg-sky-50 px-4 text-sm font-bold text-sky-700 transition hover:bg-sky-100"
                >
                  파일 선택
                </label>
                <input
                  id="media-input"
                  type="file"
                  accept="image/*,video/*"
                  onChange={(e) => setFile(e.target.files?.[0] ?? null)}
                  className="sr-only"
                />
                <span className="max-w-[220px] truncate text-sm text-muted-foreground">
                  {file ? file.name : "선택된 파일 없음"}
                </span>
                <div className="flex-1" />
                <Button type="submit" disabled={posting || !body.trim()} className="rounded-2xl">
                  {posting ? "발행 중..." : "발행"}
                </Button>
              </div>
            </form>
            </CardContent>
          </Card>
        )}

        {error && <div className="cloud-card text-sm text-destructive font-medium px-4 py-2">{error}</div>}

        {items.length === 0 && !loading && (
          <Card className="cloud-card">
            <CardContent className="py-12 text-center text-muted-foreground">
              아직 피드가 비어있어요. 첫 글을 작성하거나 다른 사람을 팔로우해 보세요.
            </CardContent>
          </Card>
        )}

        {items.map((it) => (
          <PostCard
            key={it.id}
            postId={it.id}
            authorId={it.user_id}
            authorName={it.author_name}
            title={it.title}
            body={it.body}
            visibility={it.visibility}
            createdAt={it.created_at}
          />
        ))}

        {nextCursor && (
          <div className="text-center">
            <Button variant="outline" onClick={loadMore} disabled={loading}
                    className="rounded-2xl bg-white/80 backdrop-blur">
              {loading ? "..." : "더 불러오기"}
            </Button>
          </div>
        )}
      </div>

      <aside className="space-y-4">
        <FriendsBox />
      </aside>

      {devlogOpen && (
        <DevlogDraftDialog
          selectedPosts={[]}
          onClose={() => setDevlogOpen(false)}
          onPublished={refresh}
        />
      )}
    </div>
  );
}
