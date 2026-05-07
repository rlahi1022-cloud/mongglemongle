import { useState, useMemo } from "react";
import { Link, useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { useAuth } from "@/auth/AuthContext";
import { ApiError } from "@/api/client";

export function SignupPage() {
  const { signup, loading } = useAuth();
  const nav = useNavigate();

  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [passwordConfirm, setPasswordConfirm] = useState("");
  const [displayName, setDisplayName] = useState("");
  const [error, setError] = useState<string | null>(null);

  const passwordMismatch = useMemo(
    () => passwordConfirm.length > 0 && password !== passwordConfirm,
    [password, passwordConfirm]
  );
  const canSubmit = !loading && password.length >= 4 && password === passwordConfirm
                    && email.length > 0 && displayName.length > 0;

  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    if (password !== passwordConfirm) {
      setError("비밀번호가 일치하지 않습니다.");
      return;
    }
    try {
      await signup(email, password, displayName);
      nav("/feed", { replace: true });
    } catch (err) {
      setError(err instanceof ApiError ? err.message : "회원가입 실패");
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
        </div>
        <Card className="cloud-card">
          <CardHeader>
            <CardTitle>회원가입</CardTitle>
            <CardDescription>몽글몽글에 오신 걸 환영합니다.</CardDescription>
          </CardHeader>
          <CardContent>
            <form onSubmit={onSubmit} className="space-y-4">
              <div className="space-y-2">
                <Label htmlFor="email">이메일</Label>
                <Input id="email" type="email" autoComplete="email" required className="rounded-2xl"
                  value={email} onChange={(e) => setEmail(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="display_name">표시 이름</Label>
                <Input id="display_name" required className="rounded-2xl"
                  value={displayName} onChange={(e) => setDisplayName(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="password">비밀번호 (4자 이상)</Label>
                <Input id="password" type="password" autoComplete="new-password" required minLength={4} className="rounded-2xl"
                  value={password} onChange={(e) => setPassword(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="password_confirm">비밀번호 확인</Label>
                <Input id="password_confirm" type="password" autoComplete="new-password" required minLength={4}
                  className={`rounded-2xl ${passwordMismatch ? "border-destructive focus-visible:ring-destructive" : ""}`}
                  value={passwordConfirm} onChange={(e) => setPasswordConfirm(e.target.value)} />
                {passwordMismatch && (
                  <div className="text-xs text-destructive">비밀번호가 일치하지 않습니다.</div>
                )}
                {!passwordMismatch && passwordConfirm.length > 0 && password === passwordConfirm && (
                  <div className="text-xs text-emerald-600">비밀번호 일치 ✓</div>
                )}
              </div>
              {error && <div className="text-sm text-destructive">{error}</div>}
              <Button type="submit" disabled={!canSubmit} className="w-full rounded-2xl">
                {loading ? "가입 중..." : "가입하기"}
              </Button>
            </form>
            <div className="text-sm text-muted-foreground mt-4 text-center">
              이미 계정이 있나요? <Link to="/login" className="text-primary underline">로그인</Link>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
