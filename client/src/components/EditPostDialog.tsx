import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { VisibilitySelect } from "@/components/ui/select-visibility";
import { ApiError, posts, POST_BODY_MAX, type Post, type Visibility } from "@/api/client";

interface Props {
  initial: Pick<Post, "id" | "title" | "body" | "visibility" | "category">;
  onClose: () => void;
  onSaved: (updated: Post) => void;
}

const DEVLOG_BODY_MAX = 60000;

function byteLength(value: string) {
  return new Blob([value]).size;
}

export function EditPostDialog({ initial, onClose, onSaved }: Props) {
  const isDevlog = initial.category === "devlog";
  const bodyMax = isDevlog ? DEVLOG_BODY_MAX : POST_BODY_MAX;
  const [title, setTitle] = useState(initial.title ?? "");
  const [body, setBody] = useState(initial.body);
  const [visibility, setVisibility] = useState<Visibility>(initial.visibility);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const bodySize = isDevlog ? byteLength(body) : body.length;

  const onSave = async (e: React.FormEvent) => {
    e.preventDefault();
    if (isDevlog && byteLength(body) > DEVLOG_BODY_MAX) {
      setError("개발일지 본문이 너무 깁니다. 근거 자료를 조금 줄이거나 초안을 압축해주세요.");
      return;
    }
    setSaving(true);
    setError(null);
    try {
      const patch: Partial<Pick<Post, "title" | "body" | "visibility">> = {};
      if (title !== (initial.title ?? "")) patch.title = title;
      if (body !== initial.body)            patch.body = body;
      if (visibility !== initial.visibility) patch.visibility = visibility;
      if (Object.keys(patch).length === 0) {
        onClose();
        return;
      }
      const updated = await posts.update(initial.id, patch);
      onSaved(updated);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "수정 실패");
    } finally { setSaving(false); }
  };

  return (
    <div
      className="fixed inset-0 z-50 grid place-items-center bg-black/50 backdrop-blur-sm p-4"
      onClick={onClose}
    >
      <div
        className={`cloud-card w-full p-5 space-y-4 ${isDevlog ? "max-h-[92vh] max-w-4xl overflow-y-auto" : "max-w-lg"}`}
        onClick={(e) => e.stopPropagation()}
      >
        <div>
          <h2 className="text-lg font-bold">{isDevlog ? "개발일지 수정" : "글 수정"}</h2>
          {isDevlog && (
            <div className="mt-1 text-xs text-muted-foreground">
              작성된 개발일지 초안을 Markdown 본문 그대로 편집합니다.
            </div>
          )}
        </div>
        <form onSubmit={onSave} className="space-y-3">
          <Input
            placeholder={isDevlog ? "개발일지 제목" : "제목 (선택)"}
            value={title}
            onChange={(e) => setTitle(e.target.value.slice(0, 200))}
            maxLength={200}
            className="rounded-2xl font-medium"
          />
          <Textarea
            value={body}
            onChange={(e) => setBody(e.target.value.slice(0, bodyMax))}
            rows={isDevlog ? 18 : 6}
            maxLength={bodyMax}
            className={`rounded-2xl ${isDevlog ? "min-h-[420px] font-mono text-xs leading-relaxed" : ""}`}
            placeholder={isDevlog ? "개발일지 본문" : "본문"}
          />
          <div className={`text-xs text-right ${bodySize > bodyMax * 0.9 ? "text-destructive" : "text-muted-foreground"}`}>
            {bodySize} / {bodyMax}{isDevlog ? " bytes" : ""}
          </div>
          <div className="flex items-center gap-2">
            <VisibilitySelect value={visibility} onChange={setVisibility} />
            <div className="flex-1" />
            <Button type="button" variant="ghost" onClick={onClose} className="rounded-2xl">
              취소
            </Button>
            <Button type="submit" disabled={saving || !body.trim()} className="rounded-2xl">
              {saving ? "저장 중..." : "저장"}
            </Button>
          </div>
          {error && <div className="text-sm text-destructive">{error}</div>}
        </form>
      </div>
    </div>
  );
}
