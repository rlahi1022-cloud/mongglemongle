import { Link, NavLink, Outlet, useNavigate } from "react-router-dom";
import { useState } from "react";
import { Button } from "@/components/ui/button";
import { useAuth } from "@/auth/AuthContext";
import { profile } from "@/api/client";
import { THEMES, useTheme } from "@/theme/ThemeContext";
import { NotificationsBell } from "@/components/NotificationsBell";
import { cn } from "@/lib/utils";

const navClass = ({ isActive }: { isActive: boolean }) =>
  cn(
    "px-3 py-1.5 rounded-2xl text-sm font-medium transition-all whitespace-nowrap",
    isActive
      ? "bg-primary text-primary-foreground shadow"
      : "text-foreground/80 hover:bg-primary/10 hover:text-primary"
  );

const mobileNavClass = ({ isActive }: { isActive: boolean }) =>
  cn(
    "block px-4 py-2.5 rounded-2xl text-sm transition-all",
    isActive
      ? "bg-primary text-primary-foreground"
      : "text-foreground/80 hover:bg-secondary"
  );

export function Layout() {
  const { userId, displayName, logout } = useAuth();
  const { theme, setTheme } = useTheme();
  const nav = useNavigate();
  const [themeOpen, setThemeOpen] = useState(false);
  const [menuOpen, setMenuOpen] = useState(false);
  const [bust] = useState(Date.now());

  return (
    <div className="relative min-h-screen flex flex-col">
      <div className="starfield" aria-hidden />
      <div className="starfield-extra" aria-hidden />

      <header className="relative z-20 bg-white/75 backdrop-blur-2xl border-b border-primary/20 shadow-sm">
        <div className="container flex h-16 items-center gap-3">
          {/* 모바일 햄버거 */}
          <button
            className="md:hidden p-2 rounded-xl hover:bg-secondary"
            onClick={() => setMenuOpen((v) => !v)}
            aria-label="메뉴"
          >
            <span className="block w-5 h-0.5 bg-foreground mb-1" />
            <span className="block w-5 h-0.5 bg-foreground mb-1" />
            <span className="block w-5 h-0.5 bg-foreground" />
          </button>

          <Link to="/" className="flex items-center gap-2 shrink-0">
            <img
              src="/mascot.png"
              alt=""
              className="w-10 h-10 animate-float select-none"
              draggable={false}
            />
            <span className="text-lg font-bold tracking-tight hidden sm:inline">몽글몽글</span>
          </Link>

          {/* 데스크톱 메뉴 */}
          <nav className="hidden md:flex items-center gap-1 ml-2">
            <NavLink to="/feed" className={navClass}>☁️ 피드</NavLink>
            <NavLink to="/me/timeline" className={navClass}>📓 내 글</NavLink>
            <NavLink to="/snapshot" className={navClass}>⏳ 시점 복원</NavLink>
            <NavLink to="/search" className={navClass}>🔍 검색</NavLink>
          </nav>

          <div className="flex-1" />

          {userId && <NotificationsBell />}

          {/* 테마 선택 */}
          <div className="relative hidden sm:block">
            <Button
              variant="ghost"
              size="sm"
              className="rounded-2xl"
              onClick={() => setThemeOpen((v) => !v)}
              title="테마 변경"
            >
              {theme.emoji} <span className="hidden lg:inline ml-1">{theme.label}</span>
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
              <div className="leading-tight text-sm hidden lg:block">
                <div className="font-medium">{displayName ?? `user #${userId}`}</div>
                <div className="text-xs text-muted-foreground">#{userId}</div>
              </div>
            </Link>
          )}
          <Button
            variant="ghost"
            size="sm"
            className="rounded-2xl hidden sm:inline-flex"
            onClick={async () => { await logout(); nav("/login"); }}
          >
            로그아웃
          </Button>
        </div>

        {/* 모바일 드로어 */}
        {menuOpen && (
          <div className="md:hidden border-t bg-white/95 backdrop-blur">
            <div className="container py-3 space-y-1">
              <NavLink to="/feed" className={mobileNavClass} onClick={() => setMenuOpen(false)}>☁️ 피드</NavLink>
              <NavLink to="/me/timeline" className={mobileNavClass} onClick={() => setMenuOpen(false)}>📓 내 글</NavLink>
              <NavLink to="/snapshot" className={mobileNavClass} onClick={() => setMenuOpen(false)}>⏳ 시점 복원</NavLink>
              <NavLink to="/search" className={mobileNavClass} onClick={() => setMenuOpen(false)}>🔍 검색</NavLink>
              <NavLink to="/profile" className={mobileNavClass} onClick={() => setMenuOpen(false)}>👤 프로필</NavLink>
              <button
                onClick={async () => { setMenuOpen(false); await logout(); nav("/login"); }}
                className="block w-full text-left px-4 py-2.5 rounded-2xl text-sm text-destructive hover:bg-destructive/10"
              >
                로그아웃
              </button>
            </div>
          </div>
        )}
      </header>

      <main className="relative z-10 flex-1">
        <div className="container py-6">
          <Outlet />
        </div>
      </main>
    </div>
  );
}
