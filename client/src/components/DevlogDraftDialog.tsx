import { useMemo, useState } from "react";
import { ApiError, posts, type FeedItem, type Visibility } from "@/api/client";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { VisibilitySelect } from "@/components/ui/select-visibility";
import { Textarea } from "@/components/ui/textarea";

interface Props {
  selectedPosts: FeedItem[];
  onClose: () => void;
  onPublished?: () => void;
}

type SourceKind = "naver" | "github";
type DevlogMode = "study" | "daily";

interface NaverCafeDoc {
  title: string;
  url: string;
  articleId: string;
  category: string;
  sourceType: string;
  body: string;
}

interface GitHubCommit {
  sha: string;
  message: string;
  authorDate: string;
  url: string;
}

function splitLines(raw: string) {
  return raw
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean);
}

function parseFrontMatter(raw: string) {
  const match = raw.match(/^---\n([\s\S]*?)\n---\n?/);
  const meta: Record<string, string> = {};
  if (!match) return { meta, body: raw };
  for (const line of match[1].split("\n")) {
    const sep = line.indexOf(":");
    if (sep <= 0) continue;
    const key = line.slice(0, sep).trim();
    const value = line.slice(sep + 1).trim();
    meta[key] = value;
  }
  return { meta, body: raw.slice(match[0].length) };
}

function excerpt(raw: string, max = 220) {
  const text = raw
    .replace(/^# Original Body\s*/m, "")
    .replace(/\s+/g, " ")
    .trim();
  return text.length > max ? `${text.slice(0, max)}...` : text;
}

function naverDocToLine(doc: NaverCafeDoc) {
  const label = doc.title || `article ${doc.articleId || "unknown"}`;
  const parts = [
    label,
    doc.category ? `게시판=${doc.category}` : "",
    doc.articleId ? `articleId=${doc.articleId}` : "",
    doc.sourceType ? `source=${doc.sourceType}` : "",
    doc.url,
  ].filter(Boolean);
  return `${parts.join(" | ")} :: ${excerpt(doc.body)}`;
}

function commitToLine(commit: GitHubCommit) {
  const shortSha = commit.sha.slice(0, 7);
  const date = commit.authorDate ? commit.authorDate.slice(0, 10) : "date unknown";
  const firstLine = commit.message.split("\n")[0] || "commit message empty";
  return `${shortSha} | ${date} | ${firstLine} | ${commit.url}`;
}

function sourceBlock(label: string, lines: string[]) {
  if (lines.length === 0) return `- ${label}: 제공된 근거 없음`;
  return lines.map((line, idx) => `- ${label} ${idx + 1}: ${line}`).join("\n");
}

function postEvidence(posts: FeedItem[]) {
  if (posts.length === 0) return "- 피드 글: 선택된 글 없음";
  return posts
    .map((post, idx) => {
      const title = post.title?.trim() || "제목 없음";
      const body = post.body.trim().replace(/\s+/g, " ");
      const excerpt = body.length > 140 ? `${body.slice(0, 140)}...` : body;
      return `- 피드 글 ${idx + 1}: [${post.created_at}] ${title} - ${excerpt}`;
    })
    .join("\n");
}

function evidenceSection(posts: FeedItem[], naverLines: string[], githubLines: string[]) {
  return `## 근거 자료
${postEvidence(posts)}
${sourceBlock("네이버 글", naverLines)}
${sourceBlock("GitHub 기록", githubLines)}`;
}

function safetySection() {
  return `## 과장 방지 체크
- 위 초안은 선택된 피드 글, 입력된 네이버 글, 입력된 GitHub 기록, 사용자가 적은 환경 메모만 근거로 작성됨.
- 성능 개선, 배포 완료, 버그 완전 해결처럼 근거가 필요한 표현은 입력 자료에 없으면 단정하지 않음.
- 빠진 맥락이 있으면 초안에 새 사실을 추가하기보다 근거 자료를 먼저 보강해야 함.`;
}

function buildStudyDraft(params: {
  posts: FeedItem[];
  naverLines: string[];
  githubLines: string[];
  environmentNotes: string;
  concepts: string;
  learnings: string;
  reflections: string;
  nextSteps: string;
}) {
  const { posts, naverLines, githubLines, environmentNotes, concepts, learnings, reflections, nextSteps } = params;
  const titleSeed = posts[0]?.title?.trim() || "공부형 개발일지 초안";
  const topicSeeds = posts
    .map((post) => post.title?.trim() || post.body.trim().split("\n")[0])
    .filter(Boolean)
    .slice(0, 4);

  return `# ${titleSeed}

${evidenceSection(posts, naverLines, githubLines)}

## 오늘 공부한 주제
${topicSeeds.length > 0
  ? topicSeeds.map((item) => `- ${item}`).join("\n")
  : "- 입력된 근거에서 공부 주제를 단정할 수 없어 보강 필요"}

## 개념 정리
${concepts.trim() || "제공된 개념 메모가 없어 새 개념을 임의로 설명하지 않음"}

## 배운 점
${learnings.trim() || "제공된 배운 점 메모가 없어 구체적으로 단정하지 않음"}

## 새롭게 느낀 점
${reflections.trim() || "제공된 느낌 메모가 없어 감상이나 태도를 과장하지 않음"}

## 공부 환경과 흐름
${environmentNotes.trim()}

## 더 확인할 것
${nextSteps.trim() || "제공된 다음 학습 메모가 없어 임의로 확장하지 않음"}

${safetySection()}
`;
}

function buildDailyDraft(params: {
  posts: FeedItem[];
  naverLines: string[];
  githubLines: string[];
  environmentNotes: string;
  blockers: string;
  learnings: string;
  nextSteps: string;
}) {
  const { posts, naverLines, githubLines, environmentNotes, blockers, learnings, nextSteps } = params;
  const titleSeed = posts[0]?.title?.trim() || "당일 개발 경험 초안";
  const workedOn = posts
    .map((post) => post.title?.trim() || post.body.trim().split("\n")[0])
    .filter(Boolean)
    .slice(0, 4);

  return `# ${titleSeed}

${evidenceSection(posts, naverLines, githubLines)}

## 오늘 실제로 다룬 작업
${workedOn.length > 0
  ? workedOn.map((item) => `- ${item}`).join("\n")
  : "- 선택된 피드 글이나 명시된 작업 근거가 없어 작업 항목을 단정하지 않음"}

## 개발 환경과 작업감
${environmentNotes.trim()}

## 막혔던 부분
${blockers.trim() || "제공된 막힘 기록이 없어 구체적으로 단정하지 않음"}

## 배운 점
${learnings.trim() || "제공된 회고 메모가 없어 구체적으로 단정하지 않음"}

## 다음 작업
${nextSteps.trim() || "제공된 다음 작업 메모가 없어 임의로 확장하지 않음"}

${safetySection()}
`;
}

function titleFromDraft(draft: string) {
  const firstHeading = draft.split("\n").find((line) => line.startsWith("# "));
  return firstHeading?.replace(/^#\s+/, "").trim() || "개발일지";
}

export function DevlogDraftDialog({ selectedPosts, onClose, onPublished }: Props) {
  const [mode, setMode] = useState<DevlogMode>("daily");
  const [title, setTitle] = useState(selectedPosts[0]?.title?.trim() || "개발일지");
  const [visibility, setVisibility] = useState<Visibility>("public");
  const [naverRaw, setNaverRaw] = useState("");
  const [githubRaw, setGithubRaw] = useState("");
  const [githubRepo, setGithubRepo] = useState("rlahi1022-cloud/mongglemongle");
  const [githubSince, setGithubSince] = useState("");
  const [githubUntil, setGithubUntil] = useState("");
  const [githubLoading, setGithubLoading] = useState(false);
  const [environmentNotes, setEnvironmentNotes] = useState("");
  const [blockers, setBlockers] = useState("");
  const [concepts, setConcepts] = useState("");
  const [learnings, setLearnings] = useState("");
  const [reflections, setReflections] = useState("");
  const [nextSteps, setNextSteps] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [draft, setDraft] = useState("");
  const [publishing, setPublishing] = useState(false);
  const [published, setPublished] = useState(false);

  const naverLines = useMemo(() => splitLines(naverRaw), [naverRaw]);
  const githubLines = useMemo(() => splitLines(githubRaw), [githubRaw]);

  const sourceCounts: Record<SourceKind, number> = {
    naver: naverLines.length,
    github: githubLines.length,
  };

  const importNaverFiles = async (files: FileList | null) => {
    if (!files || files.length === 0) return;
    setError(null);
    try {
      const docs: NaverCafeDoc[] = [];
      for (const file of Array.from(files)) {
        const raw = await file.text();
        const { meta, body } = parseFrontMatter(raw);
        docs.push({
          title: meta.title || file.name.replace(/\.md$/i, ""),
          url: meta.url || "",
          articleId: meta.articleId || "",
          category: meta.category || "",
          sourceType: meta.sourceType || "markdown-file",
          body,
        });
      }
      const imported = docs.map(naverDocToLine).join("\n");
      setNaverRaw((prev) => [prev.trim(), imported].filter(Boolean).join("\n"));
    } catch {
      setError("네이버 Markdown 파일을 읽지 못했습니다.");
    }
  };

  const importGithubCommits = async () => {
    const repo = githubRepo.trim();
    if (!/^[\w.-]+\/[\w.-]+$/.test(repo)) {
      setError("GitHub 저장소는 owner/repo 형식으로 입력해주세요.");
      return;
    }
    setGithubLoading(true);
    setError(null);
    try {
      const q = new URLSearchParams();
      q.set("per_page", "30");
      if (githubSince) q.set("since", new Date(githubSince).toISOString());
      if (githubUntil) q.set("until", new Date(githubUntil).toISOString());
      const resp = await fetch(`https://api.github.com/repos/${repo}/commits?${q}`);
      if (!resp.ok) throw new Error(`GitHub API ${resp.status}`);
      const payload = await resp.json() as Array<{
        sha?: string;
        html_url?: string;
        commit?: {
          message?: string;
          author?: { date?: string };
        };
      }>;
      const commits = payload.map<GitHubCommit>((item) => ({
        sha: item.sha || "",
        message: item.commit?.message || "",
        authorDate: item.commit?.author?.date || "",
        url: item.html_url || "",
      }));
      const imported = commits.map(commitToLine).join("\n");
      setGithubRaw((prev) => [prev.trim(), imported].filter(Boolean).join("\n"));
      if (commits.length === 0) setError("조건에 맞는 GitHub 커밋이 없습니다.");
    } catch (e) {
      setError(e instanceof Error ? e.message : "GitHub 기록을 가져오지 못했습니다.");
    } finally {
      setGithubLoading(false);
    }
  };

  const createDraft = () => {
    const hasEvidence = selectedPosts.length > 0 || naverLines.length > 0 || githubLines.length > 0;
    if (!hasEvidence) {
      setError("피드 글, 네이버 글, GitHub 기록 중 하나 이상의 근거가 필요합니다.");
      return;
    }
    if (environmentNotes.trim().length < 12) {
      setError("실제 개발 환경과 작업감을 조금 더 구체적으로 적어주세요.");
      return;
    }
    setError(null);
    const nextDraft = mode === "study"
      ? buildStudyDraft({
        posts: selectedPosts,
        naverLines,
        githubLines,
        environmentNotes,
        concepts,
        learnings,
        reflections,
        nextSteps,
      })
      : buildDailyDraft({
        posts: selectedPosts,
        naverLines,
        githubLines,
        environmentNotes,
        blockers,
        learnings,
        nextSteps,
      })
    ;
    setTitle(titleFromDraft(nextDraft).slice(0, 200));
    setDraft(nextDraft);
    setPublished(false);
  };

  const publishDraft = async () => {
    if (!draft.trim()) {
      setError("먼저 초안을 생성하거나 작성해주세요.");
      return;
    }
    setPublishing(true);
    setError(null);
    try {
      await posts.create(title.trim() || titleFromDraft(draft), draft.trim(), visibility, "owner_only", "devlog");
      setPublished(true);
      onPublished?.();
    } catch (e) {
      setError(e instanceof ApiError ? e.message : "개발일지 발행 실패");
    } finally {
      setPublishing(false);
    }
  };

  return (
    <div
      className="fixed inset-0 z-50 grid place-items-center bg-black/55 p-4 backdrop-blur-sm"
      onClick={onClose}
    >
      <div
        className="cloud-card flex max-h-[92vh] w-full max-w-4xl flex-col overflow-hidden"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-center justify-between border-b px-5 py-4">
          <div>
            <h2 className="text-lg font-bold">개발일지 초안</h2>
            <div className="text-xs text-muted-foreground">
              {mode === "study" ? "공부형" : "당일 개발 경험"} · {visibility} · 피드 {selectedPosts.length}개 · 네이버 {sourceCounts.naver}개 · GitHub {sourceCounts.github}개
            </div>
          </div>
          <Button type="button" variant="ghost" onClick={onClose} className="rounded-2xl">
            닫기
          </Button>
        </div>

        <div className="grid min-h-0 flex-1 grid-cols-1 gap-4 overflow-y-auto p-5 lg:grid-cols-2">
          <div className="space-y-4">
            <section className="space-y-2">
              <h3 className="text-sm font-bold">발행 설정</h3>
              <Input
                value={title}
                onChange={(e) => setTitle(e.target.value.slice(0, 200))}
                maxLength={200}
                placeholder="개발일지 제목"
                className="rounded-2xl font-medium"
              />
              <VisibilitySelect value={visibility} onChange={setVisibility} />
            </section>

            <section className="space-y-2">
              <h3 className="text-sm font-bold">개발일지 형식</h3>
              <div className="grid grid-cols-2 gap-2">
                <button
                  type="button"
                  onClick={() => setMode("study")}
                  className={`rounded-2xl border px-3 py-2 text-sm font-bold transition ${
                    mode === "study" ? "bg-primary text-primary-foreground" : "bg-white hover:bg-secondary"
                  }`}
                >
                  공부형
                </button>
                <button
                  type="button"
                  onClick={() => setMode("daily")}
                  className={`rounded-2xl border px-3 py-2 text-sm font-bold transition ${
                    mode === "daily" ? "bg-primary text-primary-foreground" : "bg-white hover:bg-secondary"
                  }`}
                >
                  당일 개발 경험
                </button>
              </div>
            </section>

            <section className="space-y-2">
              <h3 className="text-sm font-bold">네이버 글 근거</h3>
              <input
                type="file"
                accept=".md,text/markdown,text/plain"
                multiple
                onChange={(e) => importNaverFiles(e.target.files)}
                className="text-sm"
              />
              <Textarea
                value={naverRaw}
                onChange={(e) => setNaverRaw(e.target.value)}
                rows={4}
                placeholder="네이버 카페 원문 추출기에서 저장한 .md 파일을 불러오거나 URL/핵심 문장을 입력"
              />
            </section>

            <section className="space-y-2">
              <h3 className="text-sm font-bold">GitHub 기록 근거</h3>
              <div className="grid gap-2 sm:grid-cols-[1fr_120px_120px]">
                <Input
                  value={githubRepo}
                  onChange={(e) => setGithubRepo(e.target.value)}
                  placeholder="owner/repo"
                  className="rounded-2xl"
                />
                <Input
                  type="date"
                  value={githubSince}
                  onChange={(e) => setGithubSince(e.target.value)}
                  className="rounded-2xl"
                />
                <Input
                  type="date"
                  value={githubUntil}
                  onChange={(e) => setGithubUntil(e.target.value)}
                  className="rounded-2xl"
                />
              </div>
              <Button
                type="button"
                variant="outline"
                onClick={importGithubCommits}
                disabled={githubLoading}
                className="w-full rounded-2xl bg-white"
              >
                {githubLoading ? "불러오는 중..." : "GitHub 커밋 불러오기"}
              </Button>
              <Textarea
                value={githubRaw}
                onChange={(e) => setGithubRaw(e.target.value)}
                rows={4}
                placeholder="커밋, PR, 이슈, 작업 로그"
              />
            </section>

            <section className="space-y-2">
              <h3 className="text-sm font-bold">개발 환경과 작업감</h3>
              <Textarea
                value={environmentNotes}
                onChange={(e) => setEnvironmentNotes(e.target.value)}
                rows={4}
                placeholder={mode === "study"
                  ? "공부한 장소/도구, 참고 자료를 읽은 흐름, 개념을 이해할 때의 느낌"
                  : "IDE, 터미널, 빌드/실행 환경, 실제로 느낀 막힘이나 흐름"}
              />
            </section>

            {mode === "study" ? (
              <section className="space-y-2">
                <h3 className="text-sm font-bold">개념 / 배운 점 / 새롭게 느낀 점</h3>
                <Textarea
                  value={concepts}
                  onChange={(e) => setConcepts(e.target.value)}
                  rows={2}
                  placeholder="정리한 개념"
                />
                <Textarea
                  value={learnings}
                  onChange={(e) => setLearnings(e.target.value)}
                  rows={2}
                  placeholder="배운 점"
                />
                <Textarea
                  value={reflections}
                  onChange={(e) => setReflections(e.target.value)}
                  rows={2}
                  placeholder="새롭게 느낀 점"
                />
                <Textarea
                  value={nextSteps}
                  onChange={(e) => setNextSteps(e.target.value)}
                  rows={2}
                  placeholder="더 확인할 것"
                />
              </section>
            ) : (
              <section className="space-y-2">
                <h3 className="text-sm font-bold">막힘 / 배운 점 / 다음 작업</h3>
                <Textarea
                  value={blockers}
                  onChange={(e) => setBlockers(e.target.value)}
                  rows={2}
                  placeholder="막혔던 부분"
                />
                <Textarea
                  value={learnings}
                  onChange={(e) => setLearnings(e.target.value)}
                  rows={2}
                  placeholder="배운 점"
                />
                <Textarea
                  value={nextSteps}
                  onChange={(e) => setNextSteps(e.target.value)}
                  rows={2}
                  placeholder="다음 작업"
                />
              </section>
            )}

            {error && <div className="rounded-2xl bg-destructive/10 px-3 py-2 text-sm text-destructive">{error}</div>}
            {published && <div className="rounded-2xl bg-primary/10 px-3 py-2 text-sm font-medium text-primary">발행 완료</div>}

            <div className="grid grid-cols-2 gap-2">
              <Button type="button" onClick={createDraft} className="rounded-2xl">
                초안 생성
              </Button>
              <Button type="button" variant="outline" onClick={publishDraft} disabled={publishing || !draft.trim()} className="rounded-2xl bg-white">
                {publishing ? "발행 중..." : "발행"}
              </Button>
            </div>
          </div>

          <div className="flex min-h-[420px] flex-col space-y-2">
            <h3 className="text-sm font-bold">초안</h3>
            <Textarea
              value={draft}
              onChange={(e) => setDraft(e.target.value)}
              className="min-h-[380px] flex-1 font-mono text-xs leading-relaxed"
              placeholder="근거를 입력하고 초안을 생성하세요."
            />
          </div>
        </div>
      </div>
    </div>
  );
}
