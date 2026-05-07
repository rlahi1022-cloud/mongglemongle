import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { ApiError, social } from "@/api/client";

export function FriendsBox() {
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
          <div className="flex-1 rounded-2xl bg-secondary py-2">
            <div className="text-base font-bold">{followingCount ?? "—"}</div>
            <div className="text-xs text-muted-foreground">팔로잉</div>
          </div>
          <div className="flex-1 rounded-2xl bg-secondary py-2">
            <div className="text-base font-bold">{followerCount ?? "—"}</div>
            <div className="text-xs text-muted-foreground">팔로워</div>
          </div>
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
      </CardContent>
    </Card>
  );
}
