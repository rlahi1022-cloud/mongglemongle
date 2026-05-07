import { useState } from "react";
import { Link, useLocation, useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { useAuth } from "@/auth/AuthContext";
import { ApiError } from "@/api/client";

export function LoginPage() {
  const { login, loading } = useAuth();
  const nav = useNavigate();
  const loc = useLocation();
  const from = typeof loc.state === "object" && loc.state !== null && "from" in loc.state
    ? String((loc.state as { from?: unknown }).from || "/feed")
    : "/feed";

  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);

  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    try {
      await login(email, password);
      nav(from, { replace: true });
    } catch (err) {
      setError(err instanceof ApiError ? err.message : "로그인 실패");
    }
  };

  return (
    <div className="relative min-h-screen grid place-items-center px-4">
      <div className="starfield" aria-hidden />
      <div className="starfield-extra" aria-hidden />
      <div className="relative z-10 w-full max-w-sm">
        <div className="text-center mb-6">
          <img src="/mascot.png" alt="몽글몽글" className="mx-auto w-24 h-24 drop-shadow-xl animate-float" draggable={false} />
          <h1 className="text-3xl font-bold text-white mt-3 drop-shadow">몽글몽글</h1>
          <p className="text-white/70 text-sm">지나간 기록을 꺼내 오늘의 글로 잇는 시스템</p>
        </div>
        <Card className="cloud-card">
          <CardHeader>
            <CardTitle>로그인</CardTitle>
            <CardDescription>이메일과 비밀번호를 입력하세요.</CardDescription>
          </CardHeader>
          <CardContent>
            <form onSubmit={onSubmit} className="space-y-4">
              <div className="space-y-2">
                <Label htmlFor="email">이메일</Label>
                <Input id="email" type="email" autoComplete="email" required className="rounded-2xl"
                  value={email} onChange={(e) => setEmail(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="password">비밀번호</Label>
                <Input id="password" type="password" autoComplete="current-password" required className="rounded-2xl"
                  value={password} onChange={(e) => setPassword(e.target.value)} />
              </div>
              {error && (
                <div className="text-sm text-destructive">{error}</div>
              )}
              <Button type="submit" disabled={loading} className="w-full rounded-2xl">
                {loading ? "로그인 중..." : "로그인"}
              </Button>
            </form>
            <div className="text-sm text-muted-foreground mt-4 text-center">
              계정이 없으신가요? <Link to="/signup" className="text-primary underline">회원가입</Link>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
