import { useState } from "react";
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
  const [displayName, setDisplayName] = useState("");
  const [error, setError] = useState<string | null>(null);

  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    try {
      await signup(email, password, displayName);
      nav("/feed", { replace: true });
    } catch (err) {
      setError(err instanceof ApiError ? err.message : "회원가입 실패");
    }
  };

  return (
    <div className="grid place-items-center py-12">
      <Card className="w-full max-w-sm">
        <CardHeader>
          <CardTitle>회원가입</CardTitle>
          <CardDescription>몽글몽글에 오신 걸 환영합니다.</CardDescription>
        </CardHeader>
        <CardContent>
          <form onSubmit={onSubmit} className="space-y-4">
            <div className="space-y-2">
              <Label htmlFor="email">이메일</Label>
              <Input id="email" type="email" autoComplete="email" required
                value={email} onChange={(e) => setEmail(e.target.value)} />
            </div>
            <div className="space-y-2">
              <Label htmlFor="display_name">표시 이름</Label>
              <Input id="display_name" required
                value={displayName} onChange={(e) => setDisplayName(e.target.value)} />
            </div>
            <div className="space-y-2">
              <Label htmlFor="password">비밀번호</Label>
              <Input id="password" type="password" autoComplete="new-password" required minLength={4}
                value={password} onChange={(e) => setPassword(e.target.value)} />
            </div>
            {error && <div className="text-sm text-destructive">{error}</div>}
            <Button type="submit" disabled={loading} className="w-full">
              {loading ? "가입 중..." : "가입하기"}
            </Button>
          </form>
          <div className="text-sm text-muted-foreground mt-4 text-center">
            이미 계정이 있나요? <Link to="/login" className="text-primary underline">로그인</Link>
          </div>
        </CardContent>
      </Card>
    </div>
  );
}
