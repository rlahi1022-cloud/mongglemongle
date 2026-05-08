"""몽글몽글 AI 허브.

기본은 BGE-m3 임베딩 모델을 사용한다. 개발 환경에 sentence-transformers가
없거나 모델 로드에 실패하면 기존 SHA-256 64차원 stub으로 자동 fallback한다.
인터페이스(POST /embed, POST /compare, GET /healthz)는 고정되어 있어 C++
백엔드는 모델 교체와 무관하게 같은 방식으로 호출한다.

사용:
  docker compose up ai-hub
  curl http://localhost:9000/healthz
  curl -X POST http://localhost:9000/embed -H 'Content-Type: application/json' \
       -d '{"text":"오늘 Redis 만짐"}'
"""
from __future__ import annotations

import hashlib
import math
import os
from functools import lru_cache
from typing import List, Literal

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field


app = FastAPI(title="monggle-ai-hub", version="0.2.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://127.0.0.1:5173",
        "http://localhost:5173",
        "http://127.0.0.1:3000",
        "http://localhost:3000",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

EMBED_DIM = 64
DEFAULT_MODEL = os.getenv("MONGGLE_EMBEDDING_MODEL", "BAAI/bge-m3")
FORCE_STUB = os.getenv("MONGGLE_AI_STUB", "").lower() in {"1", "true", "yes"}

# ---- 모델 ---------------------------------------------------------------------

@lru_cache(maxsize=1)
def _model():
    if FORCE_STUB:
        return None
    try:
        from sentence_transformers import SentenceTransformer

        return SentenceTransformer(DEFAULT_MODEL)
    except Exception:
        return None


def _model_name() -> str:
    return DEFAULT_MODEL if _model() is not None else "stub-sha256-64d"


def _embed_dim() -> int:
    model = _model()
    if model is None:
        return EMBED_DIM
    return int(model.get_sentence_embedding_dimension() or 0)

def _hash_embed(text: str, dim: int = EMBED_DIM) -> List[float]:
    """결정적이고 가벼운 stub 임베딩.

    텍스트를 SHA-256 으로 해시한 뒤, 바이트별로 ±1 단위 벡터를 그리듯 펼쳐서
    L2 정규화. 실제 의미를 담지는 못하지만 같은 입력에 같은 벡터 + 코사인 유사도
    범위가 [-1, 1]로 정상 분포 → 백엔드 통합 검증에는 충분.
    """
    raw = hashlib.sha256(text.encode("utf-8")).digest()
    # 64차원 → 64바이트 (sha256 = 32바이트 두 번)
    extended = raw + hashlib.sha256(raw).digest()
    vec = [(b - 128) / 128.0 for b in extended[:dim]]
    norm = math.sqrt(sum(x * x for x in vec)) or 1.0
    return [x / norm for x in vec]


def _real_embed(text: str) -> List[float]:
    model = _model()
    if model is None:
        return _hash_embed(text)
    vector = model.encode(text, normalize_embeddings=True)
    return [float(x) for x in vector.tolist()]


def _cosine(a: List[float], b: List[float]) -> float:
    dot = sum(x * y for x, y in zip(a, b))
    na = math.sqrt(sum(x * x for x in a)) or 1.0
    nb = math.sqrt(sum(x * x for x in b)) or 1.0
    return dot / (na * nb)


# ---- 스키마 -------------------------------------------------------------------

class EmbedRequest(BaseModel):
    text: str = Field(min_length=1, max_length=8000)


class EmbedResponse(BaseModel):
    model: str
    dim: int
    vector: List[float]


class CompareRequest(BaseModel):
    a: str
    b: str


class CompareResponse(BaseModel):
    similarity: float


class EvidenceItem(BaseModel):
    title: str = ""
    summary: str = ""
    source: str = ""


class DevlogDraftRequest(BaseModel):
    mode: Literal["study", "daily"]
    title: str = "개발일지"
    feed: List[EvidenceItem] = Field(default_factory=list)
    naver: List[EvidenceItem] = Field(default_factory=list)
    github: List[EvidenceItem] = Field(default_factory=list)
    environment: str = ""
    concepts: str = ""
    learnings: str = ""
    reflections: str = ""
    blockers: str = ""
    next_steps: str = ""


class DevlogDraftResponse(BaseModel):
    draft: str
    model: str
    fallback: bool = True


# ---- 라우트 -------------------------------------------------------------------

@app.get("/healthz")
def healthz() -> dict:
    return {"status": "ok", "model": _model_name(), "dim": _embed_dim(), "stub": _model() is None}


@app.post("/embed", response_model=EmbedResponse)
def embed(req: EmbedRequest) -> EmbedResponse:
    vector = _real_embed(req.text)
    return EmbedResponse(model=_model_name(), dim=len(vector), vector=vector)


@app.post("/compare", response_model=CompareResponse)
def compare(req: CompareRequest) -> CompareResponse:
    sim = _cosine(_real_embed(req.a), _real_embed(req.b))
    return CompareResponse(similarity=sim)


# ---- 개발일지 초안 -------------------------------------------------------------

def _clean(text: str) -> str:
    return " ".join(text.replace("\r", " ").replace("\n", " ").split()).strip()


def _clip(text: str, max_len: int = 180) -> str:
    text = _clean(text)
    return text if len(text) <= max_len else f"{text[:max_len].rstrip()}..."


def _sentence(text: str, fallback: str) -> str:
    text = _clean(text)
    if not text:
        return fallback
    return text if text[-1] in ".!?。" else f"{text}."


def _first(items: List[str], fallback: str) -> str:
    for item in items:
        value = _clean(item)
        if len(value) >= 2:
            return value
    return fallback


def _unique(items: List[str], limit: int = 4) -> List[str]:
    seen = set()
    out: List[str] = []
    for item in items:
        value = _clean(item)
        key = value.lower()
        if not value or key in seen:
            continue
        seen.add(key)
        out.append(value)
        if len(out) >= limit:
            break
    return out


def _natural_list(items: List[str]) -> str:
    items = _unique(items, 3)
    if not items:
        return ""
    if len(items) == 1:
        return items[0]
    return ", ".join(items[:-1]) + f", 그리고 {items[-1]}"


def _evidence_title(item: EvidenceItem, fallback: str) -> str:
    return _clip(item.title or item.summary or fallback, 80)


def _memo_items(raw: str) -> List[str]:
    out: List[str] = []
    for chunk in raw.replace("·", ",").replace("/", ",").split(","):
        for line in chunk.splitlines():
            value = _clean(line)
            if value:
                out.append(value)
    return out


def _expand_learning(raw: str, fallback: str) -> str:
    item = _first(_memo_items(raw), "")
    if not item:
        return fallback
    if len(item) < 16:
        return f"{item}라는 점이다. 짧은 메모지만, 다음에 비슷한 상황을 만났을 때 판단 기준으로 다시 꺼내볼 수 있는 내용이었다."
    return _sentence(item, fallback)


def _expand_next(raw: str, fallback: str) -> str:
    item = _first(_memo_items(raw), "")
    if not item:
        return fallback
    if len(item) < 16:
        return f"{item}을 먼저 확인해 보려고 한다. 기록만 남기는 데서 끝내지 않고, 바로 검증할 수 있는 작은 단위로 이어가면 좋겠다."
    return _sentence(item, fallback)


def _work_topic(req: DevlogDraftRequest, fallback: str) -> str:
    user_memos = _memo_items(req.concepts) + _memo_items(req.learnings) + _memo_items(req.blockers)
    feed_titles = [item.title for item in req.feed]
    return _first(user_memos + feed_titles, fallback)


_DEFINITIONS = {
    "React": ("react", "리액트"),
    "Vite": ("vite",),
    "Tailwind CSS": ("tailwind", "tailwind css"),
    "TypeScript": ("typescript", "타입스크립트"),
    "Docker": ("docker", "도커"),
    "Docker Compose": ("docker compose", "compose"),
    "MariaDB": ("mariadb", "mysql"),
    "JWT": ("jwt",),
    "Redis": ("redis",),
    "FastAPI": ("fastapi",),
    "임베딩": ("embedding", "embeddings", "임베딩"),
    "BGE-m3": ("bge-m3", "bge"),
    "API": ("api",),
    "CORS": ("cors",),
    "마이그레이션": ("migration", "마이그레이션"),
    "이벤트 소싱": ("event sourcing", "이벤트 소싱"),
    "스냅샷": ("snapshot", "스냅샷"),
    "Git 커밋": ("commit", "커밋"),
}

_DEFINITION_TEXT = {
    "React": "화면을 컴포넌트 단위로 나누고 상태 변화에 맞춰 UI를 갱신하는 프론트엔드 라이브러리",
    "Vite": "개발 서버와 번들링을 빠르게 처리하는 프론트엔드 빌드 도구",
    "Tailwind CSS": "유틸리티 클래스를 조합해 화면 스타일을 만드는 CSS 프레임워크",
    "TypeScript": "JavaScript에 정적 타입을 더해 코드 오류를 더 일찍 확인하게 해주는 언어",
    "Docker": "애플리케이션과 실행 환경을 컨테이너로 묶어 일관되게 실행하게 해주는 도구",
    "Docker Compose": "여러 컨테이너를 하나의 설정으로 함께 실행하고 관리하는 도구",
    "MariaDB": "테이블과 SQL로 데이터를 다루는 MySQL 계열 관계형 데이터베이스",
    "JWT": "사용자 인증 정보를 서명된 토큰으로 주고받는 방식",
    "Redis": "캐시나 큐에 자주 쓰이는 인메모리 key-value 저장소",
    "FastAPI": "Python 타입 힌트를 활용해 API 서버를 빠르게 만드는 웹 프레임워크",
    "임베딩": "텍스트를 의미 비교가 가능한 숫자 벡터로 바꾸는 표현 방식",
    "BGE-m3": "검색과 의미 비교에 쓰이는 다국어 텍스트 임베딩 모델",
    "API": "프로그램 사이에서 기능과 데이터를 주고받는 약속된 인터페이스",
    "CORS": "브라우저가 다른 출처의 서버 요청을 허용할지 판단하는 보안 정책",
    "마이그레이션": "데이터베이스 구조나 데이터를 버전별로 안전하게 변경하는 작업",
    "이벤트 소싱": "상태 변화 이벤트를 누적해 과거 시점의 상태를 재구성하는 설계 방식",
    "스냅샷": "특정 시점의 상태를 저장해 복원 비용을 줄이는 데이터",
    "Git 커밋": "Git에서 코드 변경을 하나의 기록 단위로 저장한 것",
}


def _concept_paragraph(req: DevlogDraftRequest) -> str:
    text = " ".join([
        req.concepts,
        req.learnings,
        " ".join(item.title + " " + item.summary for item in req.feed + req.naver + req.github),
    ]).lower()
    found: List[str] = []
    for label, terms in _DEFINITIONS.items():
        if any(term in text for term in terms):
            found.append(label)
    if not found:
        return "개념 정의는 아직 충분히 정리되지 않아서, 오늘은 이해한 범위까지만 기록해 둔다."
    definitions = [f"{label}는 {_DEFINITION_TEXT[label]}이다" for label in found[:5]]
    return f"개념을 정리하면, {'. '.join(definitions)}."


def _reference_note(req: DevlogDraftRequest) -> str:
    refs: List[str] = []
    if req.feed:
        refs.append(f"피드 글 {len(req.feed)}개: {_natural_list([_evidence_title(item, '피드 글') for item in req.feed])}")
    if req.naver:
        refs.append(f"네이버 글 {len(req.naver)}개: {_natural_list([_evidence_title(item, '네이버 글') for item in req.naver])}")
    if req.github:
        refs.append(f"GitHub 기록 {len(req.github)}개: {_natural_list([_evidence_title(item, 'GitHub 기록') for item in req.github])}")
    return "\n\n## 참고한 기록\n" + "\n".join(f"- {ref}" for ref in refs) if refs else ""


def _guard_note(req: DevlogDraftRequest) -> str:
    if req.feed or req.naver or req.github:
        return "참고 기록은 초안의 배경으로만 사용했고, 기록에서 확인되지 않는 완료 여부나 성능 개선은 단정하지 않았다."
    return "입력한 기록에서 확인하기 어려운 완료, 성능 개선, 문제 해결 여부는 단정하지 않았다."


@app.post("/devlog/draft", response_model=DevlogDraftResponse)
def devlog_draft(req: DevlogDraftRequest) -> DevlogDraftResponse:
    title = _clean(req.title) or "개발일지"

    if req.mode == "study":
        concept = _work_topic(req, "오늘 공부한 내용")
        learning = _expand_learning(req.learnings, "아직 배운 점을 짧게만 남겨 두어서, 다음 기록에서 예시와 함께 더 구체화해야 한다.")
        reflection = _sentence(_first(_memo_items(req.reflections), ""), "공부하면서 생긴 생각은 이어서 더 정리할 필요가 있다.")
        next_step = _expand_next(req.next_steps, "오늘 적은 개념이 실제 코드나 작업 흐름에서 어떻게 쓰이는지 더 확인해 보려고 한다.")
        draft = f"""# {title}

오늘은 {concept}을 주제로 공부했다. 짧게 남긴 기록을 다시 읽어보니, 단어를 외우는 것보다 그 개념이 왜 필요한지 정리하는 과정이 더 중요했다.

{_concept_paragraph(req)}

참고 자료는 배경으로만 확인하고, 본문에는 내가 직접 남긴 이해와 판단을 중심으로 정리했다.

이번에 배운 점은 {learning}

공부하면서 느낀 점은 {reflection}

공부 환경과 흐름은 {_sentence(req.environment, "따로 남긴 환경 메모가 없어 자세히 정리하지 못했다.")}

다음에는 {next_step}

{_guard_note(req)}{_reference_note(req)}
"""
    else:
        work = _work_topic(req, "오늘 남긴 개발 기록")
        blocker = _sentence(_first(_memo_items(req.blockers), ""), "막혔던 부분은 아직 짧게만 남아 있어 정확한 원인까지는 단정하지 않았다.")
        learning = _expand_learning(req.learnings, "배운 점은 더 정리할 여지가 있어 다음 기록에서 코드나 상황과 함께 보강하려고 한다.")
        next_step = _expand_next(req.next_steps, "오늘 남긴 기록을 기준으로 다음 작업을 이어서 정리할 예정이다.")
        draft = f"""# {title}

오늘은 {work} 관련 작업을 진행했다. 기록을 다시 보니 작업 자체보다, 중간에 어떤 흐름으로 막히고 다시 확인했는지가 더 중요하게 남았다.

참고 자료와 커밋 기록은 작업 흐름을 확인하는 용도로만 두고, 본문은 직접 적은 메모를 기준으로 정리했다.

작업 환경은 {_sentence(req.environment, "따로 남긴 환경 메모가 없어 자세히 정리하지 못했다.")}

막혔던 부분은 {blocker}

그 과정에서 배운 점은 {learning}

다음에는 {next_step}

{_guard_note(req)}{_reference_note(req)}
"""

    return DevlogDraftResponse(draft=draft.strip() + "\n", model=_model_name())
