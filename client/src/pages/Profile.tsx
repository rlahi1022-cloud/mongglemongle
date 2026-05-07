import { useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { THEMES, useTheme } from "@/theme/ThemeContext";
import { useAuth } from "@/auth/AuthContext";
import { ApiError, profile } from "@/api/client";
import { cn } from "@/lib/utils";

type Section = "photo" | "name" | "password" | "theme";

const VERIFY_TTL_MS = 5 * 60_000; // 5분

const SECTION_META: Array<{ id: Section; label: string; emoji: string; locked: boolean }> = [
  { id: "photo",    label: "프로필 사진", emoji: "📸", locked: false },
  { id: "name",     label: "표시 이름",   emoji: "✏️", locked: true  },
  { id: "password", label: "비밀번호",    emoji: "🔑", locked: true  },
  { id: "theme",    label: "테마",        emoji: "🎨", locked: true  },
];

export function ProfilePage() {
  const { userId, displayName, email, refreshProfile } = useAuth();
  const { theme, setTheme } = useTheme();
  const nav = useNavigate();

  const [section, setSection] = useState<Section>("photo");
  const [verifiedUntil, setVerifiedUntil] = useState<number>(0);
  const verified = Date.now() < verifiedUntil;

  if (!userId) {
    nav("/login");
    return null;
  }

  const meta = SECTION_META.find((s) => s.id === section)!;
  const needGate = meta.locked && !verified;

  return (
    <div className="grid grid-cols-1 md:grid-cols-[200px_1fr] gap-5 max-w-4xl mx-auto">
      {/* 좌측 사이드바 메뉴 */}
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
            {m.locked && (
              <span className="text-xs">{verified ? "🔓" : "🔒"}</span>
            )}
          </button>
        ))}
        {verified && (
          <div className="px-3 py-2 mt-1 border-t text-xs text-emerald-600">
            잠금 해제됨 (5분간)
          </div>
        )}
      </aside>

      {/* 우측 컨텐츠 */}
      <div className="space-y-5">
        <div className="cloud-card px-5 py-4">
          <h1 className="text-2xl font-bold text-foreground flex items-center gap-2">
            <span>{meta.emoji}</span> {meta.label}
          </h1>
          <p className="text-foreground/70 text-sm mt-1">
            {meta.locked
              ? "민감한 변경입니다. 비밀번호 인증 후 변경 가능."
              : "사진은 즉시 변경됩니다."}
          </p>
        </div>

        {needGate ? (
          <PasswordGate
            onVerified={() => setVerifiedUntil(Date.now() + VERIFY_TTL_MS)}
          />
        ) : (
          <>
            {section === "photo"    && <PhotoForm userId={userId} displayName={displayName} />}
            {section === "name"     && <NameForm initial={displayName ?? ""} email={email} userId={userId}
                                                  onSaved={async () => { await refreshProfile(); }} />}
            {section === "password" && <PasswordForm onChanged={() => setVerifiedUntil(0)} />}
            {section === "theme"    && <ThemeForm theme={theme} setTheme={setTheme} />}
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
        <CardDescription>이 항목을 변경하려면 현재 비밀번호를 입력하세요. 5분간 유지됩니다.</CardDescription>
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
        <CardDescription>정사각형 256x256 jpg로 자동 가공.</CardDescription>
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
            <span className="z-0">
              {(displayName ?? "?").trim().charAt(0).toUpperCase() || "?"}
            </span>
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
    setBusy(true);
    setMsg(null);
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

function ThemeForm({ theme, setTheme }: { theme: ReturnType<typeof useTheme>["theme"]; setTheme: ReturnType<typeof useTheme>["setTheme"] }) {
  return (
    <Card className="cloud-card">
      <CardHeader>
        <CardTitle className="text-base">테마</CardTitle>
        <CardDescription>배경 그라데이션을 골라보세요.</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-2 gap-3">
          {THEMES.map((t) => (
            <button
              key={t.id}
              onClick={() => setTheme(t.id)}
              className={cn(
                "rounded-2xl border-2 p-3 text-left transition relative overflow-hidden h-24",
                t.id === theme.id ? "border-primary shadow-md" : "border-transparent hover:border-border"
              )}
              style={{ backgroundImage: t.bgGradient, backgroundSize: "100% 200%" }}
            >
              <div className="bg-white/80 backdrop-blur-sm rounded-xl px-3 py-2 inline-block">
                <div className="font-medium">{t.emoji} {t.label}</div>
                {t.id === theme.id && <div className="text-xs text-primary mt-0.5">현재 사용중</div>}
              </div>
            </button>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
