#!/usr/bin/env bash
# 몽글몽글 API 스모크 테스트
#
# - 모든 핵심 엔드포인트를 한 번씩 호출하면서 상태코드 / 응답 시간 / 검증 결과를 수집
# - 결과를 stdout(표) + tests/results/api_smoke_<timestamp>.md 로 저장
# - 의존성: bash, curl, jq
#
# 사용:
#   ./tests/api_smoke.sh                          # http://127.0.0.1:8080
#   BASE=http://127.0.0.1:8080 ./tests/api_smoke.sh
#
# 종료 코드: 실패한 케이스가 있으면 1, 모두 통과면 0.

set -uo pipefail

BASE="${BASE:-http://127.0.0.1:8080}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/results"
mkdir -p "${OUT_DIR}"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT_FILE="${OUT_DIR}/api_smoke_${STAMP}.md"
CSV_FILE="${OUT_DIR}/api_smoke_${STAMP}.csv"

# ─── 결과 누적 ─────────────────────────────────────────────────────
declare -a ROWS=()
PASS=0
FAIL=0

# ─── 유틸 ─────────────────────────────────────────────────────────
# add_row "그룹" "METHOD" "PATH" "기대" "상태코드" "응답시간ms" "PASS|FAIL" "메모"
add_row() {
  ROWS+=("$1|$2|$3|$4|$5|$6|$7|$8")
  if [ "$7" = "PASS" ]; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
}

# call <method> <path> [data] [bearer]
# echo "<status> <elapsed_ms> <body>"
call() {
  local method="$1"; shift
  local path="$1"; shift
  local data="${1:-}"
  local bearer="${2:-}"

  local args=(-sS -o /tmp/_smoke_body -w '%{http_code} %{time_total}' -X "${method}" -m 10)
  if [ -n "${bearer}" ]; then args+=(-H "Authorization: Bearer ${bearer}"); fi
  if [ -n "${data}" ];   then args+=(-H "Content-Type: application/json" -d "${data}"); fi

  local out
  out=$(curl "${args[@]}" "${BASE}${path}" 2>/dev/null) || true
  local code time_s
  code=$(echo "$out" | awk '{print $1}')
  time_s=$(echo "$out" | awk '{print $2}')
  # ms 정수
  local time_ms
  time_ms=$(awk -v t="$time_s" 'BEGIN { printf "%.0f", t*1000 }')
  local body
  body=$(cat /tmp/_smoke_body 2>/dev/null || true)
  echo "${code} ${time_ms} ${body}"
}

verify_code() {
  local got="$1" want="$2"
  if [ "$got" = "$want" ]; then echo PASS; else echo FAIL; fi
}

# ─── 사전: 백엔드 살아있는지 ─────────────────────────────────────
echo "▶ Probing backend at ${BASE} ..."
if ! curl -sS -m 2 -o /dev/null "${BASE}/healthz"; then
  echo "✗ Backend not reachable at ${BASE}. 먼저 ./build/mongglemonggle 을 띄우세요." >&2
  exit 2
fi

# ─── 1. 운영 엔드포인트 ─────────────────────────────────────────
read code ms body < <(call GET /healthz)
add_row "운영" GET /healthz 200 "$code" "$ms" "$(verify_code "$code" 200)" "$(echo "$body" | jq -r '.status // "n/a"' 2>/dev/null)"

read code ms body < <(call GET /readyz)
add_row "운영" GET /readyz 200 "$code" "$ms" "$(verify_code "$code" 200)" "db=$(echo "$body" | jq -r '.db // "?"' 2>/dev/null) redis=$(echo "$body" | jq -r '.redis // "?"' 2>/dev/null)"

read code ms body < <(call GET /metrics)
metrics_lines=$(echo "$body" | wc -l)
add_row "운영" GET /metrics 200 "$code" "$ms" "$(verify_code "$code" 200)" "${metrics_lines}lines"

# ─── 2. 인증 — 미인증 보호 라우트들 401 확인 ──────────────────
for path in /me /me/feed /me/timeline /me/followers /me/following /me/notifications /me/blocks; do
  read code ms body < <(call GET "$path")
  add_row "인증(미인증)" GET "$path" 401 "$code" "$ms" "$(verify_code "$code" 401)" ""
done

# ─── 3. 회원가입 + 로그인 페어 발급 ───────────────────────────
STAMP_USER="smoke-$(date +%s)"
EMAIL="${STAMP_USER}@monggle.local"
PW="smoke-pw-${STAMP_USER}!"
NAME="Smoke ${STAMP_USER}"

read code ms body < <(call POST /auth/signup "{\"email\":\"${EMAIL}\",\"password\":\"${PW}\",\"display_name\":\"${NAME}\"}")
add_row "인증" POST /auth/signup 201 "$code" "$ms" "$(verify_code "$code" 201)" "${EMAIL}"
ACCESS=$(echo "$body" | jq -r '.access_token // empty' 2>/dev/null)
REFRESH=$(echo "$body" | jq -r '.refresh_token // empty' 2>/dev/null)
USER_ID=$(echo "$body" | jq -r '.user_id // empty' 2>/dev/null)

if [ -z "$ACCESS" ]; then
  echo "✗ signup 응답에서 access_token 없음 — 후속 케이스 스킵" >&2
fi

# 중복 가입 → 409
read code ms body < <(call POST /auth/signup "{\"email\":\"${EMAIL}\",\"password\":\"${PW}\",\"display_name\":\"${NAME}\"}")
add_row "인증" POST /auth/signup 409 "$code" "$ms" "$(verify_code "$code" 409)" "이메일 중복 거부"

# 로그인
read code ms body < <(call POST /auth/login "{\"email\":\"${EMAIL}\",\"password\":\"${PW}\"}")
add_row "인증" POST /auth/login 200 "$code" "$ms" "$(verify_code "$code" 200)" ""
ACCESS2=$(echo "$body" | jq -r '.access_token // empty' 2>/dev/null)

# 잘못된 비밀번호
read code ms body < <(call POST /auth/login "{\"email\":\"${EMAIL}\",\"password\":\"wrong-${PW}\"}")
add_row "인증" POST /auth/login 401 "$code" "$ms" "$(verify_code "$code" 401)" "잘못된 비밀번호 거부"

# /me — 인증 필요
read code ms body < <(call GET /me "" "$ACCESS")
add_row "프로필" GET /me 200 "$code" "$ms" "$(verify_code "$code" 200)" "name=$(echo "$body" | jq -r '.display_name // "?"' 2>/dev/null)"

# ─── 4. 글 작성 / 조회 ───────────────────────────────────────
POST_BODY="API smoke test ${STAMP_USER}"
read code ms body < <(call POST /posts "{\"body\":\"${POST_BODY}\",\"visibility\":\"public\"}" "$ACCESS")
add_row "글" POST /posts 201 "$code" "$ms" "$(verify_code "$code" 201)" "id=$(echo "$body" | jq -r '.id // "?"' 2>/dev/null)"
POST_ID=$(echo "$body" | jq -r '.id // empty' 2>/dev/null)

read code ms body < <(call GET "/posts/${POST_ID}" "" "$ACCESS")
add_row "글" GET "/posts/{id}" 200 "$code" "$ms" "$(verify_code "$code" 200)" ""

# 내 타임라인
read code ms body < <(call GET /me/timeline "" "$ACCESS")
add_row "글" GET /me/timeline 200 "$code" "$ms" "$(verify_code "$code" 200)" "items=$(echo "$body" | jq -r '.items | length' 2>/dev/null)"

# 피드 (Pull) — L1+L2 캐시 검증: 첫 요청 MISS, 두 번째 요청 HIT
read code ms body < <(call GET /me/feed "" "$ACCESS")
add_row "피드" GET "/me/feed (1st)" 200 "$code" "$ms" "$(verify_code "$code" 200)" "캐시 MISS 기대"

read code ms body < <(call GET /me/feed "" "$ACCESS")
add_row "피드" GET "/me/feed (2nd)" 200 "$code" "$ms" "$(verify_code "$code" 200)" "캐시 HIT 기대"

# ─── 5. 검색 ─────────────────────────────────────────────────
read code ms body < <(call GET "/me/search?q=smoke" "" "$ACCESS")
add_row "검색" GET "/me/search?q=smoke" 200 "$code" "$ms" "$(verify_code "$code" 200)" "items=$(echo "$body" | jq -r '.items | length' 2>/dev/null)"

# ─── 6. 시점 복원 ────────────────────────────────────────────
NOW_ISO=$(date -u +%Y-%m-%dT%H:%M:%S.000Z)
read code ms body < <(call GET "/me/snapshot?at=${NOW_ISO}" "" "$ACCESS")
add_row "시점복원" GET "/me/snapshot?at=NOW" 200 "$code" "$ms" "$(verify_code "$code" 200)" "posts=$(echo "$body" | jq -r '.posts | length' 2>/dev/null)"

# ─── 7. 댓글 ─────────────────────────────────────────────────
read code ms body < <(call POST "/posts/${POST_ID}/comments" "{\"body\":\"테스트 댓글\"}" "$ACCESS")
add_row "댓글" POST "/posts/{id}/comments" 201 "$code" "$ms" "$(verify_code "$code" 201)" "id=$(echo "$body" | jq -r '.id // "?"' 2>/dev/null)"

read code ms body < <(call GET "/posts/${POST_ID}/comments" "" "$ACCESS")
add_row "댓글" GET "/posts/{id}/comments" 200 "$code" "$ms" "$(verify_code "$code" 200)" "items=$(echo "$body" | jq -r '.items | length' 2>/dev/null)"

# ─── 8. 알림 ─────────────────────────────────────────────────
read code ms body < <(call GET /me/notifications "" "$ACCESS")
add_row "알림" GET /me/notifications 200 "$code" "$ms" "$(verify_code "$code" 200)" "unread=$(echo "$body" | jq -r '.unread // "?"' 2>/dev/null)"

# ─── 9. 토큰 회전 (refresh → 새 페어) + 로그아웃 ───────────
read code ms body < <(call POST /auth/refresh "{\"refresh_token\":\"${REFRESH}\"}")
add_row "인증" POST /auth/refresh 200 "$code" "$ms" "$(verify_code "$code" 200)" "토큰 회전"
NEW_REFRESH=$(echo "$body" | jq -r '.refresh_token // empty' 2>/dev/null)

# 회전 후 옛 refresh 재사용 — 401
read code ms body < <(call POST /auth/refresh "{\"refresh_token\":\"${REFRESH}\"}")
add_row "인증" POST /auth/refresh 401 "$code" "$ms" "$(verify_code "$code" 401)" "재사용 차단"

read code ms body < <(call POST /auth/logout "{\"refresh_token\":\"${NEW_REFRESH}\"}")
add_row "인증" POST /auth/logout 204 "$code" "$ms" "$(verify_code "$code" 204)" ""

# ─── 출력 ────────────────────────────────────────────────────
TOTAL=$((PASS+FAIL))

print_table() {
  local fmt="$1"   # md|csv|console

  if [ "$fmt" = "md" ]; then
    cat <<HDR
# API 스모크 테스트 결과

- 일시: $(date '+%Y-%m-%d %H:%M:%S')
- BASE: \`${BASE}\`
- 통과: **${PASS} / ${TOTAL}** · 실패: **${FAIL}**

| # | 그룹 | METHOD | PATH | 기대 | 실제 | 응답 ms | 결과 | 메모 |
|---|---|---|---|---|---|---|---|---|
HDR
    local i=0
    for r in "${ROWS[@]}"; do
      i=$((i+1))
      IFS='|' read -r grp method path expected got ms result note <<<"$r"
      printf "| %d | %s | %s | \`%s\` | %s | %s | %s | %s | %s |\n" \
             "$i" "$grp" "$method" "$path" "$expected" "$got" "$ms" \
             "$([ "$result" = PASS ] && echo "✓ PASS" || echo "✗ FAIL")" \
             "$note"
    done
  elif [ "$fmt" = "csv" ]; then
    echo "idx,group,method,path,expected,actual,elapsed_ms,result,note"
    local i=0
    for r in "${ROWS[@]}"; do
      i=$((i+1))
      IFS='|' read -r grp method path expected got ms result note <<<"$r"
      printf "%d,%s,%s,%s,%s,%s,%s,%s,%s\n" "$i" "$grp" "$method" "$path" "$expected" "$got" "$ms" "$result" "$note"
    done
  else
    # 콘솔: 단순 표
    printf "\n%-12s %-6s %-32s %-8s %-8s %-8s %-6s %s\n" "그룹" "METHOD" "PATH" "기대" "실제" "ms" "결과" "메모"
    printf '%.s-' {1..120}; printf '\n'
    for r in "${ROWS[@]}"; do
      IFS='|' read -r grp method path expected got ms result note <<<"$r"
      printf "%-12s %-6s %-32s %-8s %-8s %-8s %-6s %s\n" \
        "$grp" "$method" "$path" "$expected" "$got" "$ms" "$result" "$note"
    done
    printf '\n총 %d개 · ✓ PASS %d · ✗ FAIL %d\n' "$TOTAL" "$PASS" "$FAIL"
  fi
}

print_table console
print_table md  > "${OUT_FILE}"
print_table csv > "${CSV_FILE}"

echo
echo "결과 저장:"
echo "  · ${OUT_FILE}"
echo "  · ${CSV_FILE}"

[ "$FAIL" -gt 0 ] && exit 1 || exit 0
