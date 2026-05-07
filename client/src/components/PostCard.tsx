import { useEffect, useState } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { media as mediaApi, profile as profileApi, type PostMedia } from "@/api/client";

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

interface Props {
  postId?: number;            // 있을 때만 미디어 fetch
  authorId?: number;          // 있을 때 아바타 표시
  authorName: string;
  body: string;
  visibility: string;
  createdAt: string;
  rightSlot?: React.ReactNode;
}

export function PostCard({ postId, authorId, authorName, body, visibility, createdAt, rightSlot }: Props) {
  const [items, setItems] = useState<PostMedia[]>([]);
  const [avatarOk, setAvatarOk] = useState(true);

  useEffect(() => {
    if (!postId) return;
    let canceled = false;
    mediaApi.listForPost(postId)
      .then((r) => { if (!canceled) setItems(r.items); })
      .catch(() => { /* ignore — 권한 없거나 미디어 없음 */ });
    return () => { canceled = true; };
  }, [postId]);

  return (
    <Card className="cloud-card">
      <CardHeader className="flex-row items-center justify-between space-y-0 pb-3">
        <div className="flex items-center gap-3">
          <div className="relative h-10 w-10 rounded-full bg-gradient-to-br from-primary to-indigo-600 text-white grid place-items-center text-sm font-bold shadow-md overflow-hidden">
            {authorId && avatarOk && (
              <img
                src={profileApi.avatarUrl(authorId)}
                alt=""
                className="absolute inset-0 w-full h-full object-cover"
                onError={() => setAvatarOk(false)}
              />
            )}
            <span className="z-0">{(authorName?.trim().charAt(0) ?? "?").toUpperCase()}</span>
          </div>
          <div>
            <CardTitle className="text-base">{authorName}</CardTitle>
            <div className="text-xs text-muted-foreground">{createdAt}</div>
          </div>
        </div>
        <div className="flex items-center gap-2">
          <span className={`text-xs px-2.5 py-0.5 rounded-full ${visBadge[visibility] ?? "bg-slate-100"}`}>
            {visLabel[visibility] ?? visibility}
          </span>
          {rightSlot}
        </div>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="whitespace-pre-wrap leading-relaxed">{body}</div>
        {items.length > 0 && (
          <div className={items.length === 1 ? "" : "grid grid-cols-2 gap-2"}>
            {items.map((m) => (
              <a
                key={m.id}
                href={mediaApi.viewUrl(m.id)}
                target="_blank"
                rel="noreferrer"
                className="block rounded-2xl overflow-hidden border bg-secondary hover:opacity-95 transition"
              >
                {m.kind === "video" ? (
                  m.has_poster ? (
                    <div className="relative">
                      <img src={mediaApi.thumbUrl(m.id)} alt="" className="w-full h-auto object-cover" />
                      <div className="absolute inset-0 grid place-items-center text-white text-3xl drop-shadow">▶</div>
                    </div>
                  ) : (
                    <div className="aspect-video grid place-items-center text-muted-foreground text-sm">🎬 video</div>
                  )
                ) : (
                  <img
                    src={m.has_thumb ? mediaApi.thumbUrl(m.id) : mediaApi.viewUrl(m.id)}
                    alt=""
                    className="w-full h-auto object-cover"
                  />
                )}
              </a>
            ))}
          </div>
        )}
      </CardContent>
    </Card>
  );
}
