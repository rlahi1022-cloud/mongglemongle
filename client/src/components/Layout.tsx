import { Link, NavLink, Outlet, useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { useAuth } from "@/auth/AuthContext";
import { profile } from "@/api/client";
import { THEMES, useTheme } from "@/theme/ThemeContext";
import { cn } from "@/lib/utils";
import { useState } from "react";

const navClass = ({ isActive }: { isActive: boolean }) =>
  cn(
    "px-3 py-1.5 rounded-2xl text-sm transition-all whitespace-nowrap",
    isActive
      ? "bg-primary text-primary-foreground shadow"
      : "text-foreground/70 hover:bg-secondary hover:text-foreground"
  );

export function Layout() {
  const { userId, displayName, logout } = useAuth();
  const { theme, setTheme } = useTheme();
  const nav = useNavigate();
  const [themeOpen, setThemeOpen] = useState(false);
  const [bust] = useState(Date.now());

  return (
    <div className="relative min-h-screen flex flex-col">
      <div className="starfield" aria-hidden />
      <div className="starfield-extra" aria-hidden />

      {/* 상단 헤더 — 한 줄 */}
      <header className="relative z-20 bg-white/85 backdrop-blur-xl border-b border-white/40 shadow-sm">
        <div className="container flex h-16 items-center gap-4">
          <Link to="/" className="flex items-center gap-2 shrink-0">
            <img
              src="/mascot.png"
              alt=""
              className="w-10 h-10 animate-float select-none"
              draggable={false}
            />
            <span className="text-lg font-bold tracking-tight">몽글몽글</span>
          </Link>

          <nav className="flex items-center gap-1 ml-2">
            <NavLink to="/feed" className={navClass}>☁️ 피드</NavLink>
            <NavLink to="/me/timeline" className={navClass}>📓 내 글</NavLink>
            <NavLink to="/snapshot" className={navClass}>⏳ 시점 복원</NavLink>
            <NavLink to="/search" className={navClass}>🔍 검색</NavLink>
          </nav>

          <div className="flex-1" />

          {/* 테마 선택 */}
          <div className="relative">
            <Button
              variant="ghost"
              size="sm"
              className="rounded-2xl"
              onClick={() => setThemeOpen((v) => !v)}
              title="테마 변경"
            >
              {theme.emoji} {theme.label}
            </Button>
            {themeOpen && (
              <div
                className="absolute right-0 mt-2 w-44 cloud-card p-2 z-30"
                onMouseLeave={() => setThemeOpen(false)}
              >
                {THEMES.map((t) => (
                  <button
                    key={t.id}
                    onClick={() => { setTheme(t.id); setThemeOpen(false); }}
                    className={cn(
                      "w-full flex items-center gap-2 px-3 py-2 rounded-xl text-sm text-left transition",
                      t.id === theme.id ? "bg-primary/10 text-primary font-medium" : "hover:bg-secondary"
                    )}
                  >
                    <span>{t.emoji}</span>
                    <span>{t.label}</span>
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* 프로필 — 클릭하면 프로필 수정 페이지 */}
          {userId && (
            <Link
              to="/profile"
              className="flex items-center gap-2 rounded-2xl px-2 py-1 hover:bg-secondary transition"
              title="프로필 수정"
            >
              <div className="relative h-9 w-9 rounded-full bg-secondary text-foreground grid place-items-center text-sm font-bold overflow-hidden">
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
              <div className="leading-tight text-sm hidden md:block">
                <div className="font-medium">{displayName ?? `user #${userId}`}</div>
                <div className="text-xs text-muted-foreground">#{userId}</div>
              </div>
            </Link>
          )}
          <Button
            variant="ghost"
            size="sm"
            className="rounded-2xl"
            onClick={async () => { await logout(); nav("/login"); }}
          >
            로그아웃
          </Button>
        </div>
      </header>

      {/* 본문 */}
      <main className="relative z-10 flex-1">
        <div className="container py-6">
          <Outlet />
        </div>
      </main>
    </div>
  );
}
