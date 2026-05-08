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
const GITHUB_REPO_KEY = "monggle_github_repo";
const DEVLOG_BODY_MAX_BYTES = 60000;

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

function normalizeGithubRepo(input: string) {
  const raw = input.trim();
  const urlMatch = raw.match(/github\.com\/([^/\s]+)\/([^/\s#?]+)/i);
  if (urlMatch) return `${urlMatch[1]}/${urlMatch[2].replace(/\.git$/, "")}`;
  return raw.replace(/^@/, "").replace(/\.git$/, "");
}

function meaningfulCharCount(value: string) {
  return (value.match(/[가-힣a-zA-Z0-9]/g) ?? []).length;
}

function hasTooMuchLooseJamo(value: string) {
  const jamo = (value.match(/[ㄱ-ㅎㅏ-ㅣ]/g) ?? []).length;
  const meaningful = meaningfulCharCount(value);
  return jamo >= 4 && jamo > meaningful;
}

function validateMeaning(label: string, value: string, required = false) {
  const trimmed = value.trim();
  if (!trimmed) return required ? `${label}을 입력해주세요.` : null;
  if (meaningfulCharCount(trimmed) < (required ? 4 : 3)) {
    return `${label}이 너무 짧거나 의미를 확인하기 어렵습니다.`;
  }
  if (hasTooMuchLooseJamo(trimmed)) {
    return `${label}에 자음/모음만 입력된 부분이 많습니다. 의미가 맞는지 확인해주세요.`;
  }
  return null;
}

function byteLength(value: string) {
  return new Blob([value]).size;
}

const CONCEPT_DEFINITIONS: Array<{ terms: string[]; label: string; definition: string }> = [
  {
    terms: ["react", "리액트"],
    label: "React",
    definition: "화면을 컴포넌트 단위로 나누고 상태 변화에 따라 UI를 다시 그리는 프론트엔드 라이브러리",
  },
  {
    terms: ["vite"],
    label: "Vite",
    definition: "개발 서버와 번들링을 빠르게 처리하는 프론트엔드 빌드 도구",
  },
  {
    terms: ["tailwind", "tailwind css"],
    label: "Tailwind CSS",
    definition: "미리 정의된 유틸리티 클래스를 조합해 화면 스타일을 빠르게 만드는 CSS 프레임워크",
  },
  {
    terms: ["shadcn", "shadcn/ui"],
    label: "shadcn/ui",
    definition: "Radix UI와 Tailwind CSS 기반 컴포넌트를 프로젝트 코드 안에 가져와 커스터마이즈하는 UI 구성 방식",
  },
  {
    terms: ["typescript", "타입스크립트"],
    label: "TypeScript",
    definition: "JavaScript에 정적 타입을 더해 컴파일 단계에서 타입 오류를 확인할 수 있게 하는 언어",
  },
  {
    terms: ["drogon"],
    label: "Drogon",
    definition: "C++로 HTTP API 서버를 만들 수 있는 비동기 웹 프레임워크",
  },
  {
    terms: ["mariadb", "mysql"],
    label: "MariaDB",
    definition: "관계형 데이터를 테이블과 SQL로 다루는 MySQL 계열 데이터베이스",
  },
  {
    terms: ["docker compose", "compose"],
    label: "Docker Compose",
    definition: "여러 컨테이너 서비스를 하나의 설정 파일로 함께 실행하고 관리하는 도구",
  },
  {
    terms: ["docker", "도커"],
    label: "Docker",
    definition: "애플리케이션과 실행 환경을 컨테이너로 묶어 일관되게 실행하게 해주는 플랫폼",
  },
  {
    terms: ["jwt"],
    label: "JWT",
    definition: "사용자 인증 정보를 서명된 토큰 형태로 주고받는 방식",
  },
  {
    terms: ["access token", "액세스 토큰"],
    label: "Access Token",
    definition: "API 요청에서 사용자를 증명하기 위해 짧은 시간 동안 사용하는 인증 토큰",
  },
  {
    terms: ["refresh token", "리프레시 토큰"],
    label: "Refresh Token",
    definition: "access token이 만료됐을 때 새 토큰을 발급받기 위해 더 길게 보관하는 토큰",
  },
  {
    terms: ["redis"],
    label: "Redis",
    definition: "캐시, 큐, 세션 저장소 등에 자주 쓰이는 인메모리 key-value 저장소",
  },
  {
    terms: ["minio"],
    label: "MinIO",
    definition: "S3와 호환되는 API를 제공하는 오브젝트 스토리지 서버",
  },
  {
    terms: ["s3"],
    label: "S3",
    definition: "파일 같은 객체 데이터를 버킷 단위로 저장하고 URL/API로 접근하는 오브젝트 스토리지",
  },
  {
    terms: ["fastapi"],
    label: "FastAPI",
    definition: "Python으로 타입 힌트 기반 API 서버를 빠르게 만들 수 있는 웹 프레임워크",
  },
  {
    terms: ["embedding", "embeddings", "임베딩"],
    label: "임베딩",
    definition: "텍스트나 데이터를 의미가 비교 가능한 숫자 벡터로 바꾸는 표현 방식",
  },
  {
    terms: ["bge-m3", "bge"],
    label: "BGE-m3",
    definition: "검색과 의미 비교에 쓰기 좋은 다국어 텍스트 임베딩 모델",
  },
  {
    terms: ["oauth"],
    label: "OAuth",
    definition: "비밀번호를 직접 공유하지 않고 외부 서비스 권한을 위임받는 인증/인가 방식",
  },
  {
    terms: ["api"],
    label: "API",
    definition: "클라이언트와 서버 또는 프로그램 사이에서 정해진 규칙으로 기능과 데이터를 주고받는 인터페이스",
  },
  {
    terms: ["cors"],
    label: "CORS",
    definition: "브라우저가 다른 출처의 서버에 요청할 때 허용 여부를 판단하는 보안 정책",
  },
  {
    terms: ["migration", "마이그레이션"],
    label: "마이그레이션",
    definition: "데이터베이스 스키마나 데이터를 버전별로 안전하게 변경하는 작업",
  },
  {
    terms: ["이벤트 소싱", "event sourcing"],
    label: "이벤트 소싱",
    definition: "현재 상태만 저장하지 않고 상태 변화 이벤트를 누적해 과거 시점의 상태를 재구성하는 설계 방식",
  },
  {
    terms: ["스냅샷", "snapshot"],
    label: "스냅샷",
    definition: "이벤트를 처음부터 모두 재생하지 않도록 특정 시점의 상태를 저장해 복원 속도를 높이는 데이터",
  },
  {
    terms: ["백프레셔", "backpressure"],
    label: "백프레셔",
    definition: "처리 속도보다 입력이 빠를 때 시스템이 과부하되지 않도록 입력량을 조절하는 흐름 제어 방식",
  },
  {
    terms: ["팬아웃", "fanout"],
    label: "팬아웃",
    definition: "하나의 이벤트나 글을 여러 사용자 피드나 대상에게 퍼뜨리는 처리 방식",
  },
  {
    terms: ["cache", "캐시"],
    label: "캐시",
    definition: "자주 쓰는 데이터를 더 빠른 저장소에 잠시 보관해 반복 조회 비용을 줄이는 방법",
  },
  {
    terms: ["like"],
    label: "LIKE 검색",
    definition: "SQL에서 특정 문자열이 포함된 행을 찾는 기본적인 패턴 검색 방식",
  },
  {
    terms: ["commit", "커밋"],
    label: "커밋",
    definition: "Git에서 코드 변경 내용을 하나의 기록 단위로 저장한 것",
  },
  {
    terms: ["pull request", "pr"],
    label: "Pull Request",
    definition: "브랜치의 변경 내용을 검토하고 병합하기 위해 요청하는 GitHub 협업 단위",
  },
  {
    terms: ["issue", "이슈"],
    label: "이슈",
    definition: "버그, 개선점, 작업 항목 등을 추적하기 위해 기록하는 관리 단위",
  },
  {
    terms: ["opencv"],
    label: "OpenCV",
    definition: "이미지 처리와 컴퓨터 비전 기능을 제공하는 라이브러리",
  },
  {
    terms: ["ffmpeg"],
    label: "ffmpeg",
    definition: "영상과 오디오를 변환하거나 분석하는 데 쓰이는 멀티미디어 도구",
  },
];

function normalizeConceptText(value: string) {
  return value.toLowerCase().replace(/\s+/g, " ").trim();
}

function extractUserConceptHints(raw: string) {
  return raw
    .split(/[\n,·/|]+/)
    .map((item) => item.replace(/(을|를|이|가|은|는)?\s*(배웠어|배웠다|공부했다|정리했다|알게 됐다|알게됨)$/g, "").trim())
    .filter((item) => meaningfulCharCount(item) >= 2)
    .slice(0, 8);
}

function splitMemoItems(raw: string) {
  return raw
    .split(/[\n,]+/)
    .map((item) => item.trim())
    .filter(Boolean);
}

function firstMeaningful(items: string[], fallback: string) {
  return items.find((item) => meaningfulCharCount(item) >= 3) || fallback;
}

function sentence(value: string, fallback: string) {
  const trimmed = value.trim();
  if (!trimmed) return fallback;
  return /[.!?。]$/.test(trimmed) ? trimmed : `${trimmed}.`;
}

function buildReferenceNote(posts: FeedItem[], naverLines: string[], githubLines: string[]) {
  const refs = [
    posts.length > 0 ? `피드 글 ${posts.length}개` : "",
    naverLines.length > 0 ? `네이버 글 ${naverLines.length}개` : "",
    githubLines.length > 0 ? `참조 GitHub ${githubLines.length}개` : "",
  ].filter(Boolean);
  if (refs.length === 0) return "";
  return `\n\n참고한 기록: ${refs.join(", ")}.`;
}

function buildDraftGuardNote() {
  return "입력한 기록에서 확인하기 어려운 완료, 성능 개선, 문제 해결 여부는 단정하지 않았다.";
}

function buildConceptMeaningLines(params: {
  posts: FeedItem[];
  naverLines: string[];
  githubLines: string[];
  concepts: string;
  learnings: string;
}) {
  const { posts, naverLines, githubLines, concepts, learnings } = params;
  const evidenceText = [
    concepts,
    learnings,
    ...posts.flatMap((post) => [post.title, post.body]),
    ...naverLines,
    ...githubLines,
  ].join("\n");
  const normalized = normalizeConceptText(evidenceText);
  const matched = CONCEPT_DEFINITIONS.filter((entry) =>
    entry.terms.some((term) => normalized.includes(normalizeConceptText(term)))
  );

  const knownLabels = new Set(matched.map((entry) => normalizeConceptText(entry.label)));
  const unknownHints = extractUserConceptHints(`${concepts}\n${learnings}`)
    .filter((hint) => !matched.some((entry) => entry.terms.some((term) =>
      normalizeConceptText(hint).includes(normalizeConceptText(term))
    )))
    .filter((hint) => !knownLabels.has(normalizeConceptText(hint)))
    .slice(0, 4);

  if (matched.length === 0 && unknownHints.length === 0) {
    return [];
  }

  const knownLines = matched
    .slice(0, 8)
    .map((entry) => `${entry.label}는 ${entry.definition}이다`);
  const unknownLines = unknownHints.map((hint) =>
    `${hint}는 배운 항목으로 남겼지만, 정확한 정의는 추가로 확인해야 한다`
  );

  return [...knownLines, ...unknownLines];
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
  const conceptItems = splitMemoItems(concepts);
  const learningItems = splitMemoItems(learnings);
  const reflectionItems = splitMemoItems(reflections);
  const nextItems = splitMemoItems(nextSteps);
  const topic = firstMeaningful([...conceptItems, ...topicSeeds], "오늘 공부한 내용");
  const conceptLines = buildConceptMeaningLines({ posts, naverLines, githubLines, concepts, learnings });
  const conceptParagraph = conceptLines.length > 0
    ? `개념을 정리하면, ${conceptLines.join(". ")}.`
    : "개념 정의는 아직 충분히 정리되지 않아서, 오늘은 이해한 범위까지만 기록해 둔다.";
  const learningParagraph = sentence(
    firstMeaningful(learningItems, ""),
    "배운 점은 아직 짧게 남아 있어, 다음 기록에서 예시와 함께 더 구체화해야 한다."
  );
  const reflectionParagraph = sentence(
    firstMeaningful(reflectionItems, ""),
    "새롭게 느낀 점은 아직 명확히 적지 않았지만, 공부하면서 생긴 생각을 이어서 정리할 필요가 있다."
  );
  const nextParagraph = sentence(
    firstMeaningful(nextItems, ""),
    "다음에는 오늘 적은 개념이 실제 코드나 작업 흐름에서 어떻게 쓰이는지 더 확인해 보려고 한다."
  );

  return `# ${titleSeed}

오늘은 ${topic}을 주제로 공부했다. 단순히 용어만 적어두기보다, 내가 이해한 범위에서 개념이 어떤 역할을 하는지 같이 정리해 보려고 했다.

${conceptParagraph}

이번에 배운 점은 ${learningParagraph}

공부하면서 느낀 점은 ${reflectionParagraph}

공부 환경과 흐름은 ${sentence(environmentNotes, "따로 남긴 환경 메모가 없어 자세히 정리하지 못했다.")}

다음에는 ${nextParagraph}

${buildDraftGuardNote()}${buildReferenceNote(posts, naverLines, githubLines)}
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
  const workSummary = firstMeaningful(workedOn, "오늘 남긴 개발 기록");
  const blockerParagraph = sentence(
    firstMeaningful(splitMemoItems(blockers), ""),
    "막혔던 부분은 아직 짧게만 남아 있어 정확한 원인까지는 단정하지 않았다."
  );
  const learningParagraph = sentence(
    firstMeaningful(splitMemoItems(learnings), ""),
    "배운 점은 더 정리할 여지가 있어 다음 기록에서 코드나 상황과 함께 보강하려고 한다."
  );
  const nextParagraph = sentence(
    firstMeaningful(splitMemoItems(nextSteps), ""),
    "다음 작업은 오늘 남긴 기록을 기준으로 이어서 정리할 예정이다."
  );

  return `# ${titleSeed}

오늘은 ${workSummary} 관련 작업을 진행했다. 기록을 다시 보니 작업 자체보다, 중간에 어떤 흐름으로 막히고 다시 확인했는지가 더 중요하게 남았다.

작업 환경은 ${sentence(environmentNotes, "따로 남긴 환경 메모가 없어 자세히 정리하지 못했다.")}

막혔던 부분은 ${blockerParagraph}

그 과정에서 배운 점은 ${learningParagraph}

다음에는 ${nextParagraph}

${buildDraftGuardNote()}${buildReferenceNote(posts, naverLines, githubLines)}
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
  const [githubRepo, setGithubRepo] = useState(
    () => localStorage.getItem(GITHUB_REPO_KEY) || "rlahi1022-cloud/mongglemongle"
  );
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
    const repo = normalizeGithubRepo(githubRepo);
    if (!/^[\w.-]+\/[\w.-]+$/.test(repo)) {
      setError("GitHub 저장소는 owner/repo 또는 GitHub URL로 입력해주세요.");
      return;
    }
    localStorage.setItem(GITHUB_REPO_KEY, repo);
    setGithubRepo(repo);
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
    const envError = validateMeaning("개발 환경과 작업감", environmentNotes, true);
    if (envError) {
      setError(`${envError} 짧게 써도 괜찮지만, 최소한 의미가 확인되어야 합니다.`);
      return;
    }
    const fieldsToCheck = mode === "study"
      ? [
        ["개념", concepts],
        ["배운 점", learnings],
        ["새롭게 느낀 점", reflections],
        ["더 확인할 것", nextSteps],
      ]
      : [
        ["막힘", blockers],
        ["배운 점", learnings],
        ["다음 작업", nextSteps],
      ];
    const invalid = fieldsToCheck
      .map(([label, value]) => validateMeaning(label, value))
      .find(Boolean);
    if (invalid) {
      setError(invalid);
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
    if (byteLength(draft) > DEVLOG_BODY_MAX_BYTES) {
      setError("개발일지 본문이 너무 깁니다. 근거 자료를 조금 줄이거나 초안을 압축해주세요.");
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
                  placeholder="owner/repo 또는 GitHub URL"
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
              <div className="text-xs text-muted-foreground">
                짧게 써도 됩니다. 예: VS Code, Docker, MariaDB 마이그레이션 확인처럼 의미가 확인되면 됩니다.
              </div>
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
                  placeholder="정리한 개념. 예: 임베딩, Docker, JWT를 배웠어"
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
            <div className="flex items-center justify-between gap-3">
              <h3 className="text-sm font-bold">초안</h3>
              <span className="text-xs text-muted-foreground">
                {byteLength(draft)} / {DEVLOG_BODY_MAX_BYTES} bytes
              </span>
            </div>
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
