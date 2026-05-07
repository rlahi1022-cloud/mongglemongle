import { Link, NavLink, Outlet, useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { useAuth } from "@/auth/AuthContext";
import { cn } from "@/lib/utils";

const navItem = ({ isActive }: { isActive: boolean }) =>
  cn(
    "px-3 py-1.5 rounded-md text-sm transition-colors",
    isActive ? "bg-accent text-accent-foreground" : "text-muted-foreground hover:text-foreground"
  );

export function Layout() {
  const { userId, displayName, logout } = useAuth();
  const nav = useNavigate();

  return (
    <div className="min-h-screen flex flex-col">
      <header className="sticky top-0 z-10 border-b bg-background/95 backdrop-blur">
        <div className="container flex h-14 items-center justify-between">
          <Link to="/" className="font-bold text-lg">
            몽글몽글 <span className="text-muted-foreground text-xs ml-1">Monggle</span>
          </Link>
          <nav className="flex items-center gap-1">
            <NavLink to="/feed" className={navItem}>피드</NavLink>
            <NavLink to="/me/timeline" className={navItem}>내 글</NavLink>
            <NavLink to="/snapshot" className={navItem}>시점 복원</NavLink>
            <NavLink to="/search" className={navItem}>검색</NavLink>
          </nav>
          <div className="flex items-center gap-2">
            {userId && (
              <div className="flex items-center gap-2">
                <div className="h-7 w-7 rounded-full bg-primary text-primary-foreground grid place-items-center text-xs font-bold">
                  {(displayName ?? "?").trim().charAt(0).toUpperCase() || "?"}
                </div>
                <div className="text-sm leading-tight">
                  <div className="font-medium">{displayName ?? `user #${userId}`}</div>
                  <div className="text-xs text-muted-foreground">#{userId}</div>
                </div>
              </div>
            )}
            <Button
              variant="outline"
              size="sm"
              onClick={async () => { await logout(); nav("/login"); }}
            >
              로그아웃
            </Button>
          </div>
        </div>
      </header>
      <main className="container flex-1 py-6">
        <Outlet />
      </main>
      <footer className="border-t py-4 text-center text-xs text-muted-foreground">
        지나간 기록을 꺼내 오늘의 글로 잇는 시스템
      </footer>
    </div>
  );
}
