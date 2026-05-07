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

export function ProfilePage() {
  const { userId, displayName, email } = useAuth();
  const { theme, setTheme } = useTheme();
  const nav = useNavigate();

  const [name, setName] = useState(displayName ?? "");
  const [savingName, setSavingName] = useState(false);
  const [nameMsg, setNameMsg] = useState<string | null>(null);

  const [bust, setBust] = useState(Date.now());
  const fileRef = useRef<HTMLInputElement>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadMsg, setUploadMsg] = useState<string | null>(null);

  if (!userId) {
    nav("/login");
    return null;
  }

  const onSaveName = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!name.trim()) return;
    setSavingName(true);
    setNameMsg(null);
    try {
      await profile.updateDisplayName(name.trim());
      setNameMsg("저장되었습니다 ✓");
      // AuthContext의 displayName도 다음 새로고침/me 갱신 시 반영
    } catch (err) {
      setNameMsg(err instanceof ApiError ? err.message : "저장 실패");
    } finally { setSavingName(false); }
  };

  const onPickAvatar = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const f = e.target.files?.[0];
    if (!f) return;
    setUploading(true);
    setUploadMsg(null);
    try {
      await profile.uploadAvatar(f);
      setBust(Date.now());
      setUploadMsg("프로필 사진이 변경되었습니다 ✓");
    } catch (err) {
      setUploadMsg(err instanceof ApiError ? err.message : "업로드 실패");
    } finally {
      setUploading(false);
      if (fileRef.current) fileRef.current.value = "";
    }
  };

  return (
    <div className="space-y-5 max-w-2xl mx-auto">
      <div className="cloud-card px-5 py-4">
        <h1 className="text-2xl font-bold text-foreground">👤 프로필 수정</h1>
        <p className="text-foreground/70 text-sm mt-1">
          프로필 사진, 표시 이름, 화면 테마를 바꿀 수 있어요.
        </p>
      </div>

      {/* 프로필 사진 */}
      <Card className="cloud-card">
        <CardHeader>
          <CardTitle className="text-base">프로필 사진</CardTitle>
          <CardDescription>정사각형으로 256x256 jpg로 자동 가공.</CardDescription>
        </CardHeader>
        <CardContent>
          <div className="flex items-center gap-4">
            <div className="relative h-24 w-24 shrink-0 rounded-full bg-secondary text-foreground grid place-items-center text-3xl font-bold overflow-hidden ring-2 ring-white shadow-md">
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
              <Button
                onClick={() => fileRef.current?.click()}
                disabled={uploading}
                className="rounded-2xl"
              >
                {uploading ? "업로드 중..." : "사진 선택"}
              </Button>
              <input
                ref={fileRef}
                type="file"
                accept="image/*"
                onChange={onPickAvatar}
                className="hidden"
              />
              {uploadMsg && (
                <div className="text-xs text-emerald-600">{uploadMsg}</div>
              )}
            </div>
          </div>
        </CardContent>
      </Card>

      {/* 표시 이름 */}
      <Card className="cloud-card">
        <CardHeader>
          <CardTitle className="text-base">표시 이름</CardTitle>
          <CardDescription>피드와 사이드바에 보이는 이름.</CardDescription>
        </CardHeader>
        <CardContent>
          <form onSubmit={onSaveName} className="flex gap-3 items-end">
            <div className="flex-1 space-y-2">
              <Label htmlFor="display_name">이름</Label>
              <Input id="display_name" value={name} onChange={(e) => setName(e.target.value)}
                     maxLength={100} className="rounded-2xl" />
            </div>
            <Button type="submit" disabled={savingName || !name.trim()} className="rounded-2xl">
              {savingName ? "저장 중..." : "저장"}
            </Button>
          </form>
          {nameMsg && (
            <div className="text-xs text-emerald-600 mt-2">{nameMsg}</div>
          )}
          <div className="text-xs text-muted-foreground mt-3">
            이메일: <span className="font-mono">{email ?? "—"}</span> · user #{userId}
          </div>
        </CardContent>
      </Card>

      {/* 테마 */}
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
                  "rounded-2xl border-2 p-3 text-left transition relative overflow-hidden",
                  t.id === theme.id ? "border-primary shadow-md" : "border-transparent hover:border-border"
                )}
                style={{ backgroundImage: t.bgGradient, backgroundSize: "100% 200%" }}
              >
                <div className="bg-white/80 backdrop-blur-sm rounded-xl px-3 py-2 inline-block">
                  <div className="font-medium">{t.emoji} {t.label}</div>
                  {t.id === theme.id && (
                    <div className="text-xs text-primary mt-0.5">현재 사용중</div>
                  )}
                </div>
              </button>
            ))}
          </div>
        </CardContent>
      </Card>
    </div>
  );
}
