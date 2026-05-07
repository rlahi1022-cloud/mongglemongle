import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { ApiError, profile, social, type UserBrief } from "@/api/client";
import { cn } from "@/lib/utils";

export function FriendsBox() {
  const [following, setFollowing] = useState<UserBrief[] | null>(null);
  const [followers, setFollowers] = useState<UserBrief[] | null>(null);
  const [tab, setTab] = useState<"following" | "followers">("following");
  const [followInput, setFollowInput] = useState("");
  const [msg, setMsg] = useState<string | null>(null);

  const refresh = async () => {
    try {
      const a = await social.following();
      const b = await social.followers();
      setFollowing(a.items);
      setFollowers(b.items);
    } catch { /* */ }
  };
  useEffect(() => { refresh(); }, []);

  const onFollow = async (e: React.FormEvent) => {
    e.preventDefault();
    const id = Number(followInput);
    if (!id) return;
    setMsg(null);
    try {
      await social.follow(id);
      setFollowInput("");
      setMsg(`#${id} 팔로우 완료 ☁️`);
      await refresh();
    } catch (err) {
      setMsg(err instanceof ApiError ? err.message : "실패");
    }
  };

  const onUnfollow = async (id: number) => {
    if (!confirm("언팔로우할까요?")) return;
    try {
      await social.unfollow(id);
      setFollowing((prev) => (prev ?? []).filter((u) => u.id !== id));
    } catch (e) {
      alert(e instanceof ApiError ? e.message : "실패");
    }
  };

  const list = tab === "following" ? following : followers;

  return (
    <Card className="cloud-card">
      <CardHeader className="pb-3">
        <CardTitle className="text-sm flex items-center justify-between">
          <span>친구</span>
          <span className="text-xs text-muted-foreground font-normal">팔로잉 / 팔로워</span>
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="flex gap-2 text-center">
          <button
            onClick={() => setTab("following")}
            className={cn(
              "flex-1 rounded-2xl py-2 transition",
              tab === "following" ? "bg-primary text-primary-foreground" : "bg-secondary"
            )}
          >
            <div className="text-base font-bold">{following?.length ?? "—"}</div>
            <div className="text-xs opacity-70">팔로잉</div>
          </button>
          <button
            onClick={() => setTab("followers")}
            className={cn(
              "flex-1 rounded-2xl py-2 transition",
              tab === "followers" ? "bg-primary text-primary-foreground" : "bg-secondary"
            )}
          >
            <div className="text-base font-bold">{followers?.length ?? "—"}</div>
            <div className="text-xs opacity-70">팔로워</div>
          </button>
        </div>

        <form onSubmit={onFollow} className="flex gap-2">
          <input
            placeholder="user id"
            value={followInput}
            onChange={(e) => setFollowInput(e.target.value)}
            className="h-9 w-full rounded-2xl bg-white border border-border placeholder:text-muted-foreground/60 px-3 text-sm focus:outline-none focus:ring-2 focus:ring-ring"
          />
          <Button type="submit" size="sm" className="rounded-2xl">팔로우</Button>
        </form>
        {msg && <div className="text-xs text-muted-foreground">{msg}</div>}

        {/* 목록 */}
        <div className="border-t pt-3">
          {!list && <div className="text-xs text-muted-foreground">불러오는 중...</div>}
          {list && list.length === 0 && (
            <div className="text-xs text-muted-foreground py-4 text-center">
              {tab === "following" ? "팔로우한 사람이 없어요." : "아직 팔로워가 없어요."}
            </div>
          )}
          <div className="space-y-1.5 max-h-72 overflow-y-auto">
            {list?.map((u) => (
              <div key={u.id} className="flex items-center gap-2 px-2 py-1.5 rounded-xl hover:bg-secondary/60 transition">
                <div className="relative h-8 w-8 shrink-0 rounded-full bg-secondary text-foreground grid place-items-center text-xs font-bold overflow-hidden">
                  <span className="z-0">{u.display_name?.charAt(0)?.toUpperCase() || "?"}</span>
                  <img
                    src={profile.avatarUrl(u.id)}
                    alt=""
                    className="absolute inset-0 w-full h-full object-cover z-10"
                    onError={(e) => { (e.currentTarget as HTMLImageElement).style.display = "none"; }}
                  />
                </div>
                <div className="flex-1 min-w-0">
                  <div className="text-sm font-medium truncate">{u.display_name}</div>
                  <div className="text-[10px] text-muted-foreground truncate">#{u.id}</div>
                </div>
                {tab === "following" && (
                  <button
                    onClick={() => onUnfollow(u.id)}
                    className="text-[10px] text-muted-foreground hover:text-destructive shrink-0"
                  >
                    언팔
                  </button>
                )}
              </div>
            ))}
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
