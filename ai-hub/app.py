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
from typing import List

from fastapi import FastAPI
from pydantic import BaseModel, Field


app = FastAPI(title="monggle-ai-hub", version="0.2.0")

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
