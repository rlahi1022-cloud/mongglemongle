import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Textarea } from "@/components/ui/textarea";
import { VisibilitySelect } from "@/components/ui/select-visibility";
import { PostCard } from "@/components/PostCard";
import {
  ApiError,
  posts,
  social,
  uploadMedia,
  mediaUrl,
  type FeedItem,
  type Visibility,
} from "@/api/client";

export function FeedPage() {
  const [items, setItems] = useState<FeedItem[]>([]);
  const [nextCursor, setNextCursor] = useState<number | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Composer
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
      const created = await posts.create(body.trim(), visibility);
      if (file) {
        try { await uploadMedia(created.id, file); } catch { /* ignore */ }
      }
      setBody("");
      setFile(null);
      (document.getElementById("media-input") as HTMLInputElement | null)?.value && (
        ((document.getElementById("media-input") as HTMLInputElement).value = "")
      );
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
    <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
      <div className="lg:col-span-2 space-y-4">
        <Card>
          <CardHeader>
            <CardTitle className="text-base">새 글</CardTitle>
          </CardHeader>
          <CardContent>
            <form onSubmit={onPost} className="space-y-3">
              <Textarea
                placeholder="오늘 무엇을 했나요? 시간이 흐른 뒤 다시 꺼내볼 한 줄."
                value={body}
                onChange={(e) => setBody(e.target.value)}
                rows={4}
              />
              <div className="flex flex-wrap items-center gap-3">
                <VisibilitySelect value={visibility} onChange={setVisibility} />
                <input
                  id="media-input"
                  type="file"
                  accept="image/*,video/*"
                  onChange={(e) => setFile(e.target.files?.[0] ?? null)}
                  className="text-sm"
                />
                <div className="flex-1" />
                <Button type="submit" disabled={posting || !body.trim()}>
                  {posting ? "발행 중..." : "발행"}
                </Button>
              </div>
            </form>
          </CardContent>
        </Card>

        {error && <div className="text-sm text-destructive">{error}</div>}

        {items.length === 0 && !loading && (
          <Card>
            <CardContent className="py-10 text-center text-muted-foreground">
              아직 피드가 비어있어요. 첫 글을 작성하거나 다른 사람을 팔로우해 보세요.
            </CardContent>
          </Card>
        )}

        {items.map((it) => (
          <PostCard
            key={it.id}
            authorName={it.author_name}
            body={it.body}
            visibility={it.visibility}
            createdAt={it.created_at}
          />
        ))}

        {nextCursor && (
          <div className="text-center">
            <Button variant="outline" onClick={loadMore} disabled={loading}>
              {loading ? "..." : "더 불러오기"}
            </Button>
          </div>
        )}
      </div>

      <aside className="space-y-4">
        <Card>
          <CardHeader><CardTitle className="text-base">친구</CardTitle></CardHeader>
          <CardContent>
            <FollowsBox />
          </CardContent>
        </Card>
      </aside>
    </div>
  );
}

function FollowsBox() {
  const [followingCount, setFollowingCount] = useState<number | null>(null);
  const [followerCount, setFollowerCount] = useState<number | null>(null);
  const [followInput, setFollowInput] = useState("");
  const [msg, setMsg] = useState<string | null>(null);

  const refresh = async () => {
    try {
      const a = await social.following();
      const b = await social.followers();
      setFollowingCount(a.items.length);
      setFollowerCount(b.items.length);
    } catch { /* */ }
  };
  useEffect(() => { refresh(); }, []);

  const doFollow = async (e: React.FormEvent) => {
    e.preventDefault();
    const id = Number(followInput);
    if (!id) return;
    setMsg(null);
    try {
      await social.follow(id);
      setFollowInput("");
      setMsg(`#${id} 팔로우 완료`);
      await refresh();
    } catch (err) {
      setMsg(err instanceof ApiError ? err.message : "실패");
    }
  };

  return (
    <div className="space-y-3 text-sm">
      <div className="flex justify-between">
        <span>팔로잉</span><span className="font-medium">{followingCount ?? "-"}</span>
      </div>
      <div className="flex justify-between">
        <span>팔로워</span><span className="font-medium">{followerCount ?? "-"}</span>
      </div>
      <form onSubmit={doFollow} className="flex gap-2 pt-2">
        <input
          placeholder="user id"
          value={followInput}
          onChange={(e) => setFollowInput(e.target.value)}
          className="h-9 w-full rounded-md border border-input bg-background px-3 text-sm"
        />
        <Button type="submit" size="sm" variant="secondary">팔로우</Button>
      </form>
      {msg && <div className="text-xs text-muted-foreground">{msg}</div>}
    </div>
  );
}

// 피드 카드에 미디어 미리보기를 붙이려면 백엔드에 list-media 엔드포인트가 필요.
// 현재는 업로드만 지원하고 카드 표시는 후속.
export const _mediaUrl = mediaUrl;
