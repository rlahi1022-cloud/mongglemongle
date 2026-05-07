import { useEffect, useState } from "react";
import { Link, NavLink, Outlet, useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { useAuth } from "@/auth/AuthContext";
import { ApiError, social } from "@/api/client";
import { cn } from "@/lib/utils";

const navClass = ({ isActive }: { isActive: boolean }) =>
  cn("nav-item", isActive && "nav-item-active");

export function Layout() {
  const { userId, displayName, logout } = useAuth();
  const nav = useNavigate();

  return (
    <div className="relative min-h-screen flex">
      {/* 별이 반짝이는 배경 레이어 (body 그라데이션 위) */}
      <div className="starfield" aria-hidden />
      <div className="starfield-extra" aria-hidden />

      {/* 좌측 사이드바 */}
      <aside className="relative z-10 w-72 shrink-0 p-5 flex flex-col gap-5 text-white">
        {/* 마스코트 + 타이틀 */}
        <Link to="/" className="flex items-center gap-3 group">
          <img
            src="/mascot.png"
            alt="몽글몽글 마스코트"
            className="w-16 h-16 drop-shadow-lg animate-float select-none"
            draggable={false}
          />
          <div className="leading-tight">
            <div className="text-2xl font-bold tracking-tight">몽글몽글</div>
            <div className="text-xs text-white/70">Monggle Monggle</div>
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
        <div className="sky-card p-3 flex items-center gap-3">
          <div className="h-10 w-10 rounded-full bg-white text-primary grid place-items-center text-base font-bold">
            {(displayName ?? "?").trim().charAt(0).toUpperCase() || "?"}
          </div>
          <div className="flex-1 leading-tight min-w-0">
            <div className="font-medium truncate">{displayName ?? `user #${userId}`}</div>
            <div className="text-xs text-white/70">#{userId}</div>
          </div>
          <Button
            variant="ghost"
            size="sm"
            className="text-white/80 hover:text-white hover:bg-white/10"
            onClick={async () => { await logout(); nav("/login"); }}
          >
            로그아웃
          </Button>
        </div>
      </aside>

      {/* 본문 */}
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
        <span className="text-white/80">친구</span>
        <span className="text-xs text-white/60">팔로잉 / 팔로워</span>
      </div>
      <div className="flex gap-3 text-center">
        <div className="flex-1 rounded-xl bg-white/10 py-2">
          <div className="text-lg font-bold">{followingCount ?? "—"}</div>
          <div className="text-xs text-white/70">팔로잉</div>
        </div>
        <div className="flex-1 rounded-xl bg-white/10 py-2">
          <div className="text-lg font-bold">{followerCount ?? "—"}</div>
          <div className="text-xs text-white/70">팔로워</div>
        </div>
      </div>
      <form onSubmit={onFollow} className="flex gap-2">
        <input
          placeholder="user id"
          value={followInput}
          onChange={(e) => setFollowInput(e.target.value)}
          className="h-9 w-full rounded-2xl bg-white/15 placeholder:text-white/50 px-3 text-sm text-white border border-white/20 focus:outline-none focus:ring-2 focus:ring-white/40"
        />
        <Button
          type="submit"
          size="sm"
          variant="secondary"
          className="rounded-2xl bg-white text-primary hover:bg-white/90"
        >
          팔로우
        </Button>
      </form>
      {msg && <div className="text-xs text-white/70">{msg}</div>}
    </div>
  );
}
