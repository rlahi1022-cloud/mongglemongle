import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { VisibilitySelect } from "@/components/ui/select-visibility";
import { ApiError, posts, POST_BODY_MAX, type Post, type Visibility } from "@/api/client";

interface Props {
  initial: Pick<Post, "id" | "title" | "body" | "visibility">;
  onClose: () => void;
  onSaved: (updated: Post) => void;
}

export function EditPostDialog({ initial, onClose, onSaved }: Props) {
  const [title, setTitle] = useState(initial.title ?? "");
  const [body, setBody] = useState(initial.body);
  const [visibility, setVisibility] = useState<Visibility>(initial.visibility);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const onSave = async (e: React.FormEvent) => {
    e.preventDefault();
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
        className="cloud-card w-full max-w-lg p-5 space-y-4"
        onClick={(e) => e.stopPropagation()}
      >
        <h2 className="text-lg font-bold">글 수정</h2>
        <form onSubmit={onSave} className="space-y-3">
          <Input
            placeholder="제목 (선택)"
            value={title}
            onChange={(e) => setTitle(e.target.value.slice(0, 200))}
            maxLength={200}
            className="rounded-2xl font-medium"
          />
          <Textarea
            value={body}
            onChange={(e) => setBody(e.target.value.slice(0, POST_BODY_MAX))}
            rows={6}
            maxLength={POST_BODY_MAX}
            className="rounded-2xl"
          />
          <div className={`text-xs text-right ${body.length > POST_BODY_MAX * 0.9 ? "text-destructive" : "text-muted-foreground"}`}>
            {body.length} / {POST_BODY_MAX}
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
