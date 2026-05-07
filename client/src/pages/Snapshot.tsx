import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ApiError, posts, type SnapshotResult } from "@/api/client";

function nowIso(): string {
  const now = new Date();
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}T${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
}

export function SnapshotPage() {
  const [at, setAt] = useState<string>(nowIso());
  const [data, setData] = useState<SnapshotResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const run = async () => {
    setLoading(true);
    setError(null);
    try {
      const r = await posts.snapshot(at);
      setData(r);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "복원 실패");
    } finally { setLoading(false); }
  };

  return (
    <div className="space-y-5">
      <div className="cloud-card px-5 py-4">
        <h1 className="text-2xl font-bold text-foreground">⏳ 시점 복원</h1>
        <p className="text-foreground/70 text-sm mt-1">
          임의 시점 t의 사용자 상태를 이벤트 소스(post_events) 재생으로 정확히 복원합니다.
          삭제·수정·공개범위 변경 모두 그 시점 그대로.
        </p>
      </div>
      <Card className="cloud-card">
        <CardHeader>
          <CardTitle className="text-base">언제로 돌아갈까요?</CardTitle>
          <CardDescription>현지 시간(UTC 변환은 서버에서 처리). datetime-local 형식.</CardDescription>
        </CardHeader>
        <CardContent>
          <div className="flex gap-3 items-end">
            <div className="flex-1 space-y-2">
              <Label htmlFor="at">시점</Label>
              <Input id="at" type="datetime-local" step="1" value={at}
                className="rounded-2xl"
                onChange={(e) => setAt(e.target.value)} />
            </div>
            <Button onClick={run} disabled={loading} className="rounded-2xl">
              {loading ? "복원 중..." : "복원"}
            </Button>
          </div>
        </CardContent>
      </Card>

      {error && <div className="cloud-card text-sm text-destructive font-medium px-4 py-2">{error}</div>}

      {data && (
        <div className="space-y-3">
          <div className="cloud-card px-4 py-2 text-sm text-foreground">
            복원 시점: <span className="font-mono">{data.target_time}</span>
            · 글 {data.posts.length}개
          </div>
          {data.posts.length === 0 && (
            <Card className="cloud-card">
              <CardContent className="py-12 text-center text-muted-foreground">
                이 시점엔 작성된 글이 아직 없었습니다.
              </CardContent>
            </Card>
          )}
          {data.posts.map((p) => (
            <Card key={p.id} className={`cloud-card ${p.deleted ? "opacity-60" : ""}`}>
              <CardHeader className="pb-2">
                <div className="flex items-center justify-between">
                  <div className="font-medium">post #{p.id}</div>
                  <div className="flex items-center gap-2 text-xs">
                    <span className="px-2 py-0.5 rounded-full bg-secondary">{p.visibility}</span>
                    {p.deleted && (
                      <span className="px-2 py-0.5 rounded-full bg-destructive text-destructive-foreground">삭제됨</span>
                    )}
                    <span className="text-muted-foreground">last_event #{p.last_event_id}</span>
                  </div>
                </div>
              </CardHeader>
              <CardContent>
                <div className="whitespace-pre-wrap">{p.body}</div>
              </CardContent>
            </Card>
          ))}
        </div>
      )}
    </div>
  );
}
