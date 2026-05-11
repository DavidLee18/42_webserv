#!/usr/bin/env zsh
# webserv_conditional_tests.zsh — Section 4.1/4.2: ETag, Last-Modified,
# conditional GET (If-None-Match, If-Modified-Since).
#
# Acceptance criteria:
#   - GET to a static resource carries ETag and Last-Modified.
#   - If-None-Match with matching tag → 304 (no body, ETag echoed).
#   - If-None-Match with non-matching tag → 200 with body.
#   - If-Modified-Since ≥ Last-Modified → 304; older → 200.
#   - HEAD respects conditional headers identically.
#
# Usage:
#   PORT=8080 TARGET_URL=/ ./webserv_conditional_tests.zsh
#   VERBOSE=1 ./webserv_conditional_tests.zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
TARGET_URL=${TARGET_URL:-/}
VERBOSE=${VERBOSE:-0}
PYTHON=${PYTHON:-$(command -v python3)}

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

PASS=0; FAIL=0
typeset -a FAILED
FAILED=()

print -- "target:    http://${HOST}:${PORT}${TARGET_URL}"
print -- "(VERBOSE=1 to dump raw responses on failure)"
print -- ""

# Issue an HTTP request with optional extra headers.
# args: method path [extra_header ...]
http_get() {
  local method=$1 path=$2; shift 2
  local extras=("$@")
  HOST=$HOST PORT=$PORT METHOD=$method P=$path \
  EXTRA="${(j:|:)extras}" "$PYTHON" - <<'PY'
import os, socket, sys
host = os.environ['HOST']; port = int(os.environ['PORT'])
m = os.environ['METHOD']; p = os.environ['P']
extras = [h for h in os.environ['EXTRA'].split('|') if h]
hdrs = [f'{m} {p} HTTP/1.1', 'Host: x', 'Connection: close', *extras]
req = ('\r\n'.join(hdrs) + '\r\n\r\n').encode()
try:
    s = socket.create_connection((host, port), timeout=5)
    s.sendall(req); data = b''
    while True:
        c = s.recv(8192)
        if not c: break
        data += c
    s.close()
    sys.stdout.buffer.write(data)
except Exception as e:
    sys.stderr.write(f'err: {e}\n')
PY
}

status_of() { print -r -- "$1" | head -n1 | awk '{print $2}'; }

# Case-insensitive header lookup; strips trailing \r and leading whitespace.
header_of() {
  print -r -- "$1" | "$PYTHON" -c '
import sys, re
name = sys.argv[1].lower()
data = sys.stdin.buffer.read()
hdr = data.split(b"\r\n\r\n", 1)[0]
for line in hdr.split(b"\r\n")[1:]:
    if b":" in line:
        k, _, v = line.partition(b":")
        if k.strip().lower() == name.encode():
            sys.stdout.write(v.strip().decode("latin-1", "replace"))
            break
' "$2"
}

body_of() {
  print -r -- "$1" | "$PYTHON" -c '
import sys
data = sys.stdin.buffer.read()
i = data.find(b"\r\n\r\n")
sys.stdout.buffer.write(data[i+4:] if i >= 0 else b"")
'
}

run_test() {
  local name=$1 fn=$2 out
  out=$($fn 2>&1); local rc=$?
  if (( rc == 0 )); then
    print -r -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— $out${C_OFF}"
    (( PASS++ ))
  elif (( rc == 2 )); then
    print -r -- "${C_DIM}SKIP $name — $out${C_OFF}"
  else
    print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— $out${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
  fi
}

# Capture baseline GET once; cache ETag and Last-Modified for reuse.
RAW=$(http_get GET "$TARGET_URL")
ETAG=$(header_of "$RAW" "ETag")
LM=$(header_of "$RAW" "Last-Modified")

# -----------------------------------------------------------------------------

t_et1_etag_present() {
  if [[ -z $ETAG ]]; then
    print -r -- "ETag header absent on GET $TARGET_URL"; return 1
  fi
  print -r -- "ETag=$ETAG"
}

t_et2_last_modified() {
  if [[ -z $LM ]]; then
    print -r -- "Last-Modified header absent"; return 1
  fi
  local ok
  ok=$(DATE="$LM" "$PYTHON" -c '
import os, sys, email.utils
sys.exit(0 if email.utils.parsedate_tz(os.environ["DATE"]) else 1)
' && echo y || echo n)
  if [[ $ok != y ]]; then
    print -r -- "Last-Modified='$LM' (not parseable)"; return 1
  fi
  print -r -- "Last-Modified='$LM'"
}

t_et3_inm_match() {
  [[ -z $ETAG ]] && { print -r -- "skipped (no ETag emitted)"; return 2; }
  local raw st
  raw=$(http_get GET "$TARGET_URL" "If-None-Match: $ETAG")
  st=$(status_of "$raw")
  if [[ $st != 304 ]]; then
    print -r -- "If-None-Match: $ETAG → $st (expected 304)"; return 1
  fi
  print -r -- "If-None-Match match → 304"
}

t_et4_inm_nomatch() {
  local raw st
  raw=$(http_get GET "$TARGET_URL" 'If-None-Match: "deadbeefcafe"')
  st=$(status_of "$raw")
  if [[ $st != 200 ]]; then
    print -r -- "If-None-Match wrong → $st (expected 200)"; return 1
  fi
  print -r -- "If-None-Match wrong → 200"
}

t_et5_ims_after() {
  [[ -z $LM ]] && { print -r -- "skipped (no Last-Modified)"; return 2; }
  local raw st
  raw=$(http_get GET "$TARGET_URL" "If-Modified-Since: $LM")
  st=$(status_of "$raw")
  if [[ $st != 304 ]]; then
    print -r -- "If-Modified-Since: $LM → $st (expected 304)"; return 1
  fi
  print -r -- "If-Modified-Since equal → 304"
}

t_et6_ims_before() {
  local raw st
  raw=$(http_get GET "$TARGET_URL" 'If-Modified-Since: Sun, 01 Jan 1995 00:00:00 GMT')
  st=$(status_of "$raw")
  if [[ $st != 200 ]]; then
    print -r -- "If-Modified-Since 1995 → $st (expected 200)"; return 1
  fi
  print -r -- "If-Modified-Since old → 200"
}

t_et7_304_no_body() {
  [[ -z $ETAG ]] && { print -r -- "skipped (no ETag)"; return 2; }
  local raw body
  raw=$(http_get GET "$TARGET_URL" "If-None-Match: $ETAG")
  body=$(body_of "$raw")
  if [[ -n $body ]]; then
    print -r -- "304 has body (${#body} bytes)"; return 1
  fi
  print -r -- "304 body empty"
}

t_et8_304_carries_etag() {
  [[ -z $ETAG ]] && { print -r -- "skipped (no ETag)"; return 2; }
  local raw etag2
  raw=$(http_get GET "$TARGET_URL" "If-None-Match: $ETAG")
  etag2=$(header_of "$raw" "ETag")
  if [[ -z $etag2 ]]; then
    print -r -- "304 missing ETag"; return 1
  fi
  print -r -- "304 carries ETag=$etag2"
}

t_et9_etag_stable() {
  [[ -z $ETAG ]] && { print -r -- "skipped (no ETag)"; return 2; }
  local raw etag2
  raw=$(http_get GET "$TARGET_URL")
  etag2=$(header_of "$raw" "ETag")
  if [[ $ETAG != $etag2 ]]; then
    print -r -- "ETag drifted: $ETAG vs $etag2"; return 1
  fi
  print -r -- "ETag stable across requests"
}

t_et10_inm_star() {
  local raw st
  raw=$(http_get GET "$TARGET_URL" 'If-None-Match: *')
  st=$(status_of "$raw")
  if [[ $st != 304 ]]; then
    print -r -- "If-None-Match: * → $st (expected 304 if resource exists)"; return 1
  fi
  print -r -- "If-None-Match: * → 304"
}

t_et11_head_conditional() {
  [[ -z $ETAG ]] && { print -r -- "skipped (no ETag)"; return 2; }
  local raw st
  raw=$(http_get HEAD "$TARGET_URL" "If-None-Match: $ETAG")
  st=$(status_of "$raw")
  if [[ $st != 304 ]]; then
    print -r -- "HEAD If-None-Match → $st (expected 304)"; return 1
  fi
  print -r -- "HEAD honours conditional → 304"
}

t_et12_ims_malformed() {
  # RFC 9110 §13.1.3: malformed If-Modified-Since must be ignored, return 200.
  local raw st
  raw=$(http_get GET "$TARGET_URL" 'If-Modified-Since: not-a-date')
  st=$(status_of "$raw")
  if [[ $st != 200 ]]; then
    print -r -- "malformed IMS → $st (expected 200; should be ignored)"; return 1
  fi
  print -r -- "malformed IMS ignored → 200"
}

run_test "ET1  ETag present on 200"               t_et1_etag_present
run_test "ET2  Last-Modified present, parseable"  t_et2_last_modified
run_test "ET3  If-None-Match (match) → 304"       t_et3_inm_match
run_test "ET4  If-None-Match (no match) → 200"    t_et4_inm_nomatch
run_test "ET5  If-Modified-Since (≥) → 304"       t_et5_ims_after
run_test "ET6  If-Modified-Since (<) → 200"       t_et6_ims_before
run_test "ET7  304 has empty body"                t_et7_304_no_body
run_test "ET8  304 carries ETag"                  t_et8_304_carries_etag
run_test "ET9  ETag stable across requests"       t_et9_etag_stable
run_test "ET10 If-None-Match: * → 304"            t_et10_inm_star
run_test "ET11 HEAD honours conditional"          t_et11_head_conditional
run_test "ET12 malformed IMS ignored"             t_et12_ims_malformed

print -- ""
print -- "──────────────────────────────────────────────────────────────"
print -- "passed: $PASS   failed: $FAIL"
if (( FAIL > 0 )); then
  print -- ""
  print -- "failures:"
  for f in "${FAILED[@]}"; do print -- "  - $f"; done
fi
exit $FAIL
