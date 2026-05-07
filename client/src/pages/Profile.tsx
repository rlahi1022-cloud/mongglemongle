import { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { useAuth } from "@/auth/AuthContext";
import { ApiError, blocks, profile, social, type BlockedUser, type UserBrief } from "@/api/client";
import { cn } from "@/lib/utils";

type Section = "photo" | "name" | "password" | "friends" | "blocks";

const VERIFY_TTL_MS = 15 * 60_000; // 15분

const SECTION_META: Array<{ id: Section; label: string; emoji: string; locked: boolean }> = [
  { id: "photo",    label: "프로필 사진", emoji: "📸", locked: false },
  { id: "name",     label: "표시 이름",   emoji: "✏️", locked: true  },
  { id: "password", label: "비밀번호",    emoji: "🔑", locked: true  },
  { id: "friends",  label: "친구 목록",   emoji: "👥", locked: false },
  { id: "blocks",   label: "블랙리스트",  emoji: "🚫", locked: true  },
];

export function ProfilePage() {
  const { userId, displayName, email, refreshProfile } = useAuth();
  const nav = useNavigate();

  const [section, setSection] = useState<Section>("photo");
  const [verifiedUntil, setVerifiedUntil] = useState<number>(0);
  const [, setTick] = useState(0);
  // 잠금 만료 표시 갱신을 위한 1초 틱
  useEffect(() => {
    const id = window.setInterval(() => setTick((t) => t + 1), 1000);
    return () => window.clearInterval(id);
  }, []);
  const verified = Date.now() < verifiedUntil;
  const remainingMin = verified ? Math.ceil((verifiedUntil - Date.now()) / 60_000) : 0;

  if (!userId) {
    nav("/login");
    return null;
  }

  const meta = SECTION_META.find((s) => s.id === section)!;
  const needGate = meta.locked && !verified;

  return (
    <div className="grid grid-cols-1 md:grid-cols-[200px_1fr] gap-5 max-w-4xl mx-auto">
      <aside className="cloud-card p-2 h-fit md:sticky md:top-20">
        <div className="px-3 py-2 text-xs text-muted-foreground">프로필</div>
        {SECTION_META.map((m) => (
          <button
            key={m.id}
            onClick={() => setSection(m.id)}
            className={cn(
              "w-full flex items-center gap-2 px-3 py-2.5 rounded-xl text-sm text-left transition",
              section === m.id ? "bg-primary/10 text-primary font-medium" : "hover:bg-secondary"
            )}
          >
            <span>{m.emoji}</span>
            <span className="flex-1">{m.label}</span>
            {m.locked && <span className="text-xs">{verified ? "🔓" : "🔒"}</span>}
          </button>
        ))}
        {verified && (
          <div className="px-3 py-2 mt-1 border-t text-xs text-emerald-600">
            잠금 해제 (~{remainingMin}분)
          </div>
        )}
      </aside>

      <div className="space-y-5">
        <div className="cloud-card px-5 py-4">
          <h1 className="text-2xl font-bold text-foreground flex items-center gap-2">
            <span>{meta.emoji}</span> {meta.label}
          </h1>
          <p className="text-foreground/70 text-sm mt-1">
            {meta.locked
              ? "민감한 변경입니다. 비밀번호 인증 후 15분간 잠금 해제."
              : section === "friends"
              ? "내가 팔로우한 사람과 나를 팔로우한 사람."
              : "사진은 즉시 변경됩니다."}
          </p>
        </div>

        {needGate ? (
          <PasswordGate onVerified={() => setVerifiedUntil(Date.now() + VERIFY_TTL_MS)} />
        ) : (
          <>
            {section === "photo"    && <PhotoForm userId={userId} displayName={displayName} />}
            {section === "name"     && <NameForm initial={displayName ?? ""} email={email} userId={userId}
                                                  onSaved={refreshProfile} />}
            {section === "password" && <PasswordForm onChanged={() => setVerifiedUntil(0)} />}
            {section === "friends"  && <FriendsList />}
            {section === "blocks"   && <BlocksList />}
          </>
        )}
      </div>
    </div>
  );
}

function PasswordGate({ onVerified }: { onVerified: () => void }) {
  const [pw, setPw] = useState("");
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setBusy(true);
    setErr(null);
    try {
      const r = await profile.verifyPassword(pw);
      if (!r.ok) throw new ApiError(401, "invalid", "비밀번호가 다릅니다.");
      onVerified();
    } catch (e) {
      setErr(e instanceof ApiError ? e.message : "확인 실패");
    } finally { setBusy(false); }
  };
  return (
    <Card className="cloud-card">
      <CardHeader>
        <CardTitle className="text-base">🔒 비밀번호 인증 필요</CardTitle>
        <CardDescription>이 항목을 변경하려면 현재 비밀번호를 입력하세요. 15분간 유지됩니다.</CardDescription>
      </CardHeader>
      <CardContent>
        <form onSubmit={onSubmit} className="space-y-3">
          <div className="space-y-2">
            <Label htmlFor="gate_pw">현재 비밀번호</Label>
            <Input id="gate_pw" type="password" required autoFocus className="rounded-2xl"
                   value={pw} onChange={(e) => setPw(e.target.value)} />
          </div>
          {err && <div className="text-sm text-destructive">{err}</div>}
          <Button type="submit" disabled={busy || !pw} className="rounded-2xl">
            {busy ? "확인 중..." : "잠금 해제"}
          </Button>
        </form>
      </CardContent>
    </Card>
  );
}

function PhotoForm({ userId, displayName }: { userId: number; displayName: string | null }) {
  const fileRef = useRef<HTMLInputElement>(null);
  const [bust, setBust] = useState(Date.now());
  const [uploading, setUploading] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);

  const onPick = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const f = e.target.files?.[0];
    if (!f) return;
    setUploading(true);
    setMsg(null);
    try {
      await profile.uploadAvatar(f);
      setBust(Date.now());
      setMsg("프로필 사진이 변경되었습니다 ✓");
    } catch (err) {
      setMsg(err instanceof ApiError ? err.message : "업로드 실패");
    } finally {
      setUploading(false);
      if (fileRef.current) fileRef.current.value = "";
    }
  };

  return (
    <Card className="cloud-card">
      <CardHeader>
        <CardTitle className="text-base">사진</CardTitle>
        <CardDescription>jpg/png/webp 등. 정사각형 256x256 jpg로 자동 가공.</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="flex items-center gap-4">
          <div className="relative h-28 w-28 shrink-0 rounded-full bg-secondary text-foreground grid place-items-center text-4xl font-bold overflow-hidden ring-2 ring-white shadow-md">
            <img
              src={profile.avatarUrl(userId, bust)}
              alt=""
              className="absolute inset-0 w-full h-full object-cover"
              onError={(e) => { (e.currentTarget as HTMLImageElement).style.display = "none"; }}
            />
            <span className="z-0">{(displayName ?? "?").trim().charAt(0).toUpperCase() || "?"}</span>
          </div>
          <div className="space-y-2">
            <Button onClick={() => fileRef.current?.click()} disabled={uploading} className="rounded-2xl">
              {uploading ? "업로드 중..." : "사진 선택"}
            </Button>
            <input ref={fileRef} type="file" accept="image/*" onChange={onPick} className="hidden" />
            {msg && (
              <div className={cn("text-xs", msg.includes("변경되었") ? "text-emerald-600" : "text-destructive")}>{msg}</div>
            )}
          </div>
        </div>
      </CardContent>
    </Card>
  );
}

function NameForm({ initial, email, userId, onSaved }: {
  initial: string; email: string | null; userId: number; onSaved: () => Promise<void>;
}) {
  const [name, setName] = useState(initial);
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);
  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!name.trim()) return;
    setBusy(true); setMsg(null);
    try {
      await profile.updateDisplayName(name.trim());
      await onSaved();
      setMsg("저장되었습니다 ✓");
    } catch (err) {
      setMsg(err instanceof ApiError ? err.message : "저장 실패");
    } finally { setBusy(false); }
  };
  return (
    <Card className="cloud-card">
      <CardHeader>
        <CardTitle className="text-base">표시 이름</CardTitle>
        <CardDescription>피드와 헤더에 보이는 이름.</CardDescription>
      </CardHeader>
      <CardContent>
        <form onSubmit={onSubmit} className="flex gap-3 items-end">
          <div className="flex-1 space-y-2">
            <Label htmlFor="display_name">이름</Label>
            <Input id="display_name" value={name} onChange={(e) => setName(e.target.value)} maxLength={100} className="rounded-2xl" />
          </div>
          <Button type="submit" disabled={busy || !name.trim()} className="rounded-2xl">
            {busy ? "저장 중..." : "저장"}
          </Button>
        </form>
        {msg && <div className={cn("text-xs mt-2", msg.includes("저장되었") ? "text-emerald-600" : "text-destructive")}>{msg}</div>}
        <div className="text-xs text-muted-foreground mt-3">
          이메일: <span className="font-mono">{email ?? "—"}</span> · user #{userId}
        </div>
      </CardContent>
    </Card>
  );
}

function PasswordForm({ onChanged }: { onChanged: () => void }) {
  const [oldPw, setOldPw] = useState("");
  const [newPw, setNewPw] = useState("");
  const [newPw2, setNewPw2] = useState("");
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);
  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setMsg(null);
    if (newPw.length < 4) { setMsg("새 비밀번호는 4자 이상."); return; }
    if (newPw !== newPw2) { setMsg("새 비밀번호 확인이 일치하지 않습니다."); return; }
    setBusy(true);
    try {
      await profile.changePassword(oldPw, newPw);
      setMsg("비밀번호 변경 완료 ✓ — 다음 로그인부터 새 비밀번호 사용");
      setOldPw(""); setNewPw(""); setNewPw2("");
      onChanged();
    } catch (err) {
      setMsg(err instanceof ApiError ? err.message : "변경 실패");
    } finally { setBusy(false); }
  };
  return (
    <Card className="cloud-card">
      <CardHeader>
        <CardTitle className="text-base">비밀번호</CardTitle>
        <CardDescription>변경 시 다른 기기 세션은 모두 끊깁니다.</CardDescription>
      </CardHeader>
      <CardContent>
        <form onSubmit={onSubmit} className="space-y-3">
          <div className="space-y-2">
            <Label htmlFor="old_pw">현재 비밀번호</Label>
            <Input id="old_pw" type="password" required value={oldPw} onChange={(e) => setOldPw(e.target.value)} className="rounded-2xl" />
          </div>
          <div className="space-y-2">
            <Label htmlFor="new_pw">새 비밀번호 (4자 이상)</Label>
            <Input id="new_pw" type="password" required minLength={4} value={newPw} onChange={(e) => setNewPw(e.target.value)} className="rounded-2xl" />
          </div>
          <div className="space-y-2">
            <Label htmlFor="new_pw2">새 비밀번호 확인</Label>
            <Input id="new_pw2" type="password" required minLength={4} value={newPw2} onChange={(e) => setNewPw2(e.target.value)} className="rounded-2xl" />
          </div>
          <Button type="submit" disabled={busy || !oldPw || !newPw} className="rounded-2xl">
            {busy ? "변경 중..." : "비밀번호 변경"}
          </Button>
        </form>
        {msg && <div className={cn("text-xs mt-2", msg.includes("완료") ? "text-emerald-600" : "text-destructive")}>{msg}</div>}
      </CardContent>
    </Card>
  );
}

function FriendsList() {
  const [following, setFollowing] = useState<UserBrief[] | null>(null);
  const [followers, setFollowers] = useState<UserBrief[] | null>(null);
  const [tab, setTab] = useState<"following" | "followers">("following");

  const refresh = async () => {
    try {
      const a = await social.following();
      const b = await social.followers();
      setFollowing(a.items);
      setFollowers(b.items);
    } catch { /* */ }
  };
  useEffect(() => { refresh(); }, []);

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
      <CardHeader>
        <CardTitle className="text-base">친구</CardTitle>
        <CardDescription>팔로잉/팔로워 목록.</CardDescription>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="flex gap-2">
          <button
            onClick={() => setTab("following")}
            className={cn(
              "px-3 py-1.5 rounded-2xl text-sm font-medium transition",
              tab === "following" ? "bg-primary text-primary-foreground" : "bg-secondary text-foreground/70 hover:bg-secondary/80"
            )}
          >
            팔로잉 {following?.length ?? "—"}
          </button>
          <button
            onClick={() => setTab("followers")}
            className={cn(
              "px-3 py-1.5 rounded-2xl text-sm font-medium transition",
              tab === "followers" ? "bg-primary text-primary-foreground" : "bg-secondary text-foreground/70 hover:bg-secondary/80"
            )}
          >
            팔로워 {followers?.length ?? "—"}
          </button>
        </div>
        {!list && <div className="text-sm text-muted-foreground">불러오는 중...</div>}
        {list && list.length === 0 && (
          <div className="text-sm text-muted-foreground py-6 text-center">아무도 없어요.</div>
        )}
        <div className="space-y-2">
          {list?.map((u) => (
            <div key={u.id} className="flex items-center gap-3 px-3 py-2 rounded-2xl bg-secondary/50">
              <div className="relative h-9 w-9 shrink-0 rounded-full bg-white text-primary grid place-items-center text-sm font-bold overflow-hidden">
                <img
                  src={profile.avatarUrl(u.id)}
                  alt=""
                  className="absolute inset-0 w-full h-full object-cover"
                  onError={(e) => { (e.currentTarget as HTMLImageElement).style.display = "none"; }}
                />
                <span className="z-0">{u.display_name?.charAt(0)?.toUpperCase() || "?"}</span>
              </div>
              <div className="flex-1 min-w-0">
                <div className="font-medium truncate">{u.display_name}</div>
                <div className="text-xs text-muted-foreground truncate">#{u.id} · {u.email}</div>
              </div>
              {tab === "following" && (
                <button
                  onClick={() => onUnfollow(u.id)}
                  className="text-xs text-muted-foreground hover:text-destructive"
                >
                  언팔로우
                </button>
              )}
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}

function BlocksList() {
  const [items, setItems] = useState<BlockedUser[] | null>(null);
  const [input, setInput] = useState("");
  const [msg, setMsg] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const refresh = async () => {
    try {
      const r = await blocks.list();
      setItems(r.items);
    } catch { /* */ }
  };
  useEffect(() => { refresh(); }, []);

  const onAdd = async (e: React.FormEvent) => {
    e.preventDefault();
    const id = Number(input);
    if (!id) return;
    setBusy(true); setMsg(null);
    try {
      await blocks.add(id);
      setInput("");
      setMsg(`#${id} 차단 완료`);
      await refresh();
    } catch (err) {
      setMsg(err instanceof ApiError ? err.message : "실패");
    } finally { setBusy(false); }
  };

  const onRemove = async (id: number) => {
    if (!confirm(`#${id} 차단을 해제할까요?`)) return;
    try {
      await blocks.remove(id);
      setItems((prev) => (prev ?? []).filter((u) => u.id !== id));
    } catch (e) {
      alert(e instanceof ApiError ? e.message : "실패");
    }
  };

  return (
    <Card className="cloud-card">
      <CardHeader>
        <CardTitle className="text-base">블랙리스트</CardTitle>
        <CardDescription>
          차단된 사용자의 글은 피드에 안 보이고, 서로 follow 관계도 자동 해제됩니다.
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-3">
        <form onSubmit={onAdd} className="flex gap-2">
          <Input
            placeholder="차단할 user id"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            className="rounded-2xl"
          />
          <Button type="submit" disabled={busy || !input} className="rounded-2xl">차단</Button>
        </form>
        {msg && <div className="text-xs text-muted-foreground">{msg}</div>}
        {!items && <div className="text-sm text-muted-foreground">불러오는 중...</div>}
        {items && items.length === 0 && (
          <div className="text-sm text-muted-foreground py-6 text-center">차단한 사람이 없어요.</div>
        )}
        <div className="space-y-2">
          {items?.map((u) => (
            <div key={u.id} className="flex items-center gap-3 px-3 py-2 rounded-2xl bg-destructive/5">
              <div className="relative h-9 w-9 shrink-0 rounded-full bg-secondary text-foreground grid place-items-center text-sm font-bold overflow-hidden">
                <span>{u.display_name?.charAt(0)?.toUpperCase() || "?"}</span>
              </div>
              <div className="flex-1 min-w-0">
                <div className="font-medium truncate">{u.display_name}</div>
                <div className="text-xs text-muted-foreground truncate">#{u.id} · {u.email} · {u.blocked_at.split(" ")[0]}</div>
              </div>
              <button
                onClick={() => onRemove(u.id)}
                className="text-xs text-primary hover:underline"
              >
                해제
              </button>
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
