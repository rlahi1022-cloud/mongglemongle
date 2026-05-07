"""몽글몽글 AI 허브 — MVP stub.

기획 5장의 정석은 BGE-m3 (1024차원) 임베딩 + 외부 LLM 라우팅이지만, 모델
다운로드/추론은 무거우므로 MVP 단계에서는 결정적인 해시 기반 64차원 벡터로
대체. 인터페이스(POST /embed, GET /healthz)는 동일하게 유지하므로 후속에
sentence-transformers 또는 자체 BGE-m3로 1줄 교체 가능.

사용:
  docker compose up ai-hub
  curl http://localhost:9000/healthz
  curl -X POST http://localhost:9000/embed -H 'Content-Type: application/json' \
       -d '{"text":"오늘 Redis 만짐"}'
"""
from __future__ import annotations

import hashlib
import math
from typing import List

from fastapi import FastAPI
from pydantic import BaseModel, Field


app = FastAPI(title="monggle-ai-hub", version="0.1.0-stub")

EMBED_DIM = 64

# ---- 모델 stub ----------------------------------------------------------------

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


def _cosine(a: List[float], b: List[float]) -> float:
    dot = sum(x * y for x, y in zip(a, b))
    na = math.sqrt(sum(x * x for x in a)) or 1.0
    nb = math.sqrt(sum(x * x for x in b)) or 1.0
    return dot / (na * nb)


# ---- 스키마 -------------------------------------------------------------------

class EmbedRequest(BaseModel):
    text: str = Field(min_length=1, max_length=8000)


class EmbedResponse(BaseModel):
    model: str = "stub-sha256-64d"
    dim: int = EMBED_DIM
    vector: List[float]


class CompareRequest(BaseModel):
    a: str
    b: str


class CompareResponse(BaseModel):
    similarity: float


# ---- 라우트 -------------------------------------------------------------------

@app.get("/healthz")
def healthz() -> dict:
    return {"status": "ok", "model": "stub-sha256-64d", "dim": EMBED_DIM}


@app.post("/embed", response_model=EmbedResponse)
def embed(req: EmbedRequest) -> EmbedResponse:
    return EmbedResponse(vector=_hash_embed(req.text))


@app.post("/compare", response_model=CompareResponse)
def compare(req: CompareRequest) -> CompareResponse:
    sim = _cosine(_hash_embed(req.a), _hash_embed(req.b))
    return CompareResponse(similarity=sim)
