import { useEffect, useRef, useState } from "react";
import { Link, NavLink, Outlet, useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { useAuth } from "@/auth/AuthContext";
import { ApiError, profile, social } from "@/api/client";
import { cn } from "@/lib/utils";

const navClass = ({ isActive }: { isActive: boolean }) =>
  cn("nav-item", isActive && "nav-item-active");

export function Layout() {
  const { userId, displayName, logout } = useAuth();
  const nav = useNavigate();
  const fileRef = useRef<HTMLInputElement>(null);
  const [avatarBust, setAvatarBust] = useState(Date.now());
  const [uploadingAvatar, setUploadingAvatar] = useState(false);

  const onPickAvatar = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const f = e.target.files?.[0];
    if (!f) return;
    setUploadingAvatar(true);
    try {
      await profile.uploadAvatar(f);
      setAvatarBust(Date.now()); // cache bust
    } catch (err) {
      alert(err instanceof Error ? err.message : "업로드 실패");
    } finally {
      setUploadingAvatar(false);
      if (fileRef.current) fileRef.current.value = "";
    }
  };

  const avatarUrl = userId ? profile.avatarUrl(userId, avatarBust) : "";

  return (
    <div className="flex h-screen overflow-hidden">
      {/* 별 + 그라데이션은 body에 있고, 사이드바가 흰색으로 그 위를 덮음 */}
      <div className="starfield" aria-hidden />
      <div className="starfield-extra" aria-hidden />

      {/* 좌측 사이드바 — 흰 영역, 풀 높이 고정 */}
      <aside className="relative z-20 w-72 shrink-0 h-screen bg-white shadow-[8px_0_24px_-8px_rgba(15,23,42,0.15)] flex flex-col gap-4 p-5 overflow-y-auto">
        {/* 마스코트 + 타이틀 */}
        <Link to="/" className="flex items-center gap-3">
          <img
            src="/mascot.png"
            alt="몽글몽글 마스코트"
            className="w-12 h-12 animate-float select-none"
            draggable={false}
          />
          <div className="leading-tight">
            <div className="text-lg font-bold tracking-tight text-foreground">몽글몽글</div>
            <div className="text-[11px] text-muted-foreground">Monggle Monggle</div>
          </div>
        </Link>

        {/* 메뉴 */}
        <nav className="flex flex-col gap-1">
          <NavLink to="/feed" className={navClass}>
            <span className="text-base">☁️</span>
            피드
          </NavLink>
          <NavLink to="/me/timeline" className={navClass}>
            <span className="text-base">📓</span>
            내 글
          </NavLink>
          <NavLink to="/snapshot" className={navClass}>
            <span className="text-base">⏳</span>
            시점 복원
          </NavLink>
          <NavLink to="/search" className={navClass}>
            <span className="text-base">🔍</span>
            검색
          </NavLink>
        </nav>

        <div className="flex-1" />

        {/* 친구 박스 */}
        <FriendsBox />

        {/* 프로필 + 로그아웃 */}
        <div className="border-t pt-4 flex items-center gap-3">
          <button
            type="button"
            onClick={() => fileRef.current?.click()}
            className="relative h-12 w-12 rounded-full bg-secondary text-foreground grid place-items-center text-base font-bold overflow-hidden hover:opacity-90 transition"
            title="프로필 사진 변경"
          >
            <img
              src={avatarUrl}
              alt=""
              className="absolute inset-0 w-full h-full object-cover"
              onError={(e) => { (e.currentTarget as HTMLImageElement).style.display = "none"; }}
            />
            <span className="z-0">
              {(displayName ?? "?").trim().charAt(0).toUpperCase() || "?"}
            </span>
            {uploadingAvatar && (
              <span className="absolute inset-0 bg-black/40 grid place-items-center text-xs text-white">···</span>
            )}
          </button>
          <input
            ref={fileRef}
            type="file"
            accept="image/*"
            onChange={onPickAvatar}
            className="hidden"
          />
          <div className="flex-1 leading-tight min-w-0">
            <div className="font-medium truncate">{displayName ?? `user #${userId}`}</div>
            <div className="text-xs text-muted-foreground">#{userId}</div>
          </div>
          <Button
            variant="ghost"
            size="sm"
            onClick={async () => { await logout(); nav("/login"); }}
          >
            로그아웃
          </Button>
        </div>
      </aside>

      {/* 본문 — 그라데이션 위에 카드들 */}
      <main className="relative z-10 flex-1 p-6 overflow-y-auto">
        <div className="mx-auto max-w-2xl">
          <Outlet />
        </div>
      </main>
    </div>
  );
}

function FriendsBox() {
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
    <div className="sky-card p-4 space-y-3">
      <div className="flex items-center justify-between text-sm">
        <span className="font-medium">친구</span>
        <span className="text-xs text-muted-foreground">팔로잉 / 팔로워</span>
      </div>
      <div className="flex gap-3 text-center">
        <div className="flex-1 rounded-xl bg-white/70 py-2">
          <div className="text-lg font-bold">{followingCount ?? "—"}</div>
          <div className="text-xs text-muted-foreground">팔로잉</div>
        </div>
        <div className="flex-1 rounded-xl bg-white/70 py-2">
          <div className="text-lg font-bold">{followerCount ?? "—"}</div>
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
    </div>
  );
}
