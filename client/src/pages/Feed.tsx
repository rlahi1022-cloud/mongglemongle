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
      <div className="min-w-0">
        {/* 피드 목록 — 위쪽에서 자연 스크롤. 컴포저는 화면 하단 sticky로 항상 보임 */}
        <div className="space-y-3 pb-32">
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
            <div className="text-center pt-2">
              <Button variant="outline" size="sm" onClick={loadMore} disabled={loading}
                      className="rounded-md bg-white/80 backdrop-blur">
                {loading ? "..." : "더 불러오기"}
              </Button>
            </div>
          )}
        </div>

        {/* 화면 하단 sticky 컴포저 — 글이 수백개여도 항상 손에 닿는 위치 */}
        <div className="sticky bottom-2 z-20 mt-4 space-y-2">
          <div className="cloud-card flex flex-wrap items-center gap-2 px-4 py-2.5 border border-white/40 shadow-lg">
            <div className="text-sm font-semibold text-foreground">글쓰기</div>
            <div className="text-xs text-muted-foreground hidden sm:inline">새 기록 한 줄 — 시점 복원 대상.</div>
            <div className="flex-1" />
            <Button
              type="button"
              size="sm"
              variant={composerOpen ? "secondary" : "default"}
              onClick={() => setComposerOpen((v) => !v)}
              className="rounded-md h-8 px-3 text-sm"
            >
              {composerOpen ? "닫기" : "새 글"}
            </Button>
            <Button
              type="button"
              size="sm"
              variant="outline"
              onClick={() => setDevlogOpen(true)}
              className="rounded-md h-8 border-sky-200 bg-sky-50 px-3 text-sm font-medium text-sky-700 hover:bg-sky-100"
            >
              개발일지
            </Button>
          </div>

          {composerOpen && (
            <Card className="cloud-card border border-white/40 shadow-lg">
              <CardHeader className="pb-2 pt-3">
                <CardTitle className="text-sm font-semibold">새 글 작성</CardTitle>
              </CardHeader>
              <CardContent className="pb-3">
                <form onSubmit={onPost} className="space-y-2.5">
                  <Input
                    placeholder="제목 (선택)"
                    value={title}
                    onChange={(e) => setTitle(e.target.value.slice(0, 200))}
                    maxLength={200}
                    className="rounded-md font-medium h-9"
                  />
                  <Textarea
                    placeholder="오늘 무엇을 했나요? 시간이 흐른 뒤 다시 꺼내볼 한 줄."
                    value={body}
                    onChange={(e) => setBody(e.target.value.slice(0, POST_BODY_MAX))}
                    rows={3}
                    maxLength={POST_BODY_MAX}
                    className="rounded-md"
                  />
                  <div className={`text-xs text-right ${body.length > POST_BODY_MAX * 0.9 ? "text-destructive" : "text-muted-foreground"}`}>
                    {body.length} / {POST_BODY_MAX}
                  </div>
                  <div className="flex flex-wrap items-center gap-2">
                    <VisibilitySelect value={visibility} onChange={setVisibility} />
                    <label
                      htmlFor="media-input"
                      className="inline-flex h-9 cursor-pointer items-center rounded-md border border-sky-200 bg-sky-50 px-3 text-sm font-medium text-sky-700 transition hover:bg-sky-100"
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
                    <span className="max-w-[180px] truncate text-xs text-muted-foreground">
                      {file ? file.name : "선택된 파일 없음"}
                    </span>
                    <div className="flex-1" />
                    <Button type="submit" size="sm" disabled={posting || !body.trim()} className="rounded-md h-9">
                      {posting ? "발행 중..." : "발행"}
                    </Button>
                  </div>
                </form>
              </CardContent>
            </Card>
          )}
        </div>
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
