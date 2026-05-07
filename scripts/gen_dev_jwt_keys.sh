#!/usr/bin/env bash
# 로컬 개발용 RS256 키쌍 생성. 운영에서는 KMS/Secrets Manager로 주입할 것.
# 결과: keys/dev_jwt_private.pem, keys/dev_jwt_public.pem
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KEYS_DIR="$REPO_ROOT/keys"

mkdir -p "$KEYS_DIR"

if [[ -f "$KEYS_DIR/dev_jwt_private.pem" && -f "$KEYS_DIR/dev_jwt_public.pem" ]]; then
    echo "[gen_dev_jwt_keys] 이미 존재. 새로 만들려면 keys/ 안의 파일을 지우세요."
    exit 0
fi

openssl genpkey -algorithm RSA -out "$KEYS_DIR/dev_jwt_private.pem" -pkeyopt rsa_keygen_bits:2048
openssl rsa -in "$KEYS_DIR/dev_jwt_private.pem" -pubout -out "$KEYS_DIR/dev_jwt_public.pem"

chmod 600 "$KEYS_DIR/dev_jwt_private.pem"
chmod 644 "$KEYS_DIR/dev_jwt_public.pem"

echo "[gen_dev_jwt_keys] 생성 완료:"
ls -l "$KEYS_DIR"
