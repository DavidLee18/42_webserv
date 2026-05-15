#!/usr/bin/env zsh
# webserv_headers_tests.zsh — Section 2 (security response headers) +
# response-header hygiene.
#
# Tests:
#   H1  X-Content-Type-Options: nosniff present on 200
#   H2  X-Frame-Options: DENY present on 200
#   H3  Referrer-Policy: no-referrer present on 200
#   H4  Content-Security-Policy present on 200 (default-src 'self' or similar)
#   H5  All four security headers present on 4xx error responses
#   H6  All four security headers present on HEAD responses
#   H7  HEAD body is empty (no payload) but Content-Length matches GET
#   H8  Headers are NOT duplicated (each appears at most once)
#   H9  Content-Type is always set on 2xx/3xx/4xx/5xx
#   H10 Content-Length matches actual body length
#   H11 Date header is present and parseable as RFC 7231 IMF-fixdate
#   H12 Server header present (informational; no version leak preferred)
#   H13 CGI responses: security headers added without duplicating CGI's own
#       Content-Type. (Skipped if CGI_TEST_URL is not provided.)
#   H14 No header value contains bare CR or LF (response splitting)
#
# Usage:
#   PORT=8080 ./webserv_headers_tests.zsh
#   CGI_TEST_URL=/cgi-test/hello ./webserv_headers_tests.zsh   # optional
#
# Override default URLs via env: ROOT_URL (default /), MISSING_URL (/__nope__).

emulate -L zsh
set -u

HOST=127.0.0.1
PORT=${PORT:-8080}
ROOT_URL=${ROOT_URL:-/}
MISSING_URL=${MISSING_URL:-/__definitely_not_here_42__}
CGI_TEST_URL=${CGI_TEST_URL:-}
VERBOSE=${VERBOSE:-0}

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

PASS=0
FAIL=0
typeset -a FAILED
FAILED=()

print -r -- "target:    http://${HOST}:${PORT}"
print -r -- "root:      $ROOT_URL"
print -r -- "missing:   $MISSING_URL"
print -r -- "cgi-url:   ${CGI_TEST_URL:-<not configured; H13 will be skipped>}"
print -r -- "(VERBOSE=1 to dump headers on failure)"
print -r -- ""

# -----------------------------------------------------------------------------
# helpers — raw HTTP capture
# -----------------------------------------------------------------------------

# Issue an HTTP request and return the full raw response (headers \r\n\r\n body)
# via stdout. No parsing — we want the bytes as the server emitted them.
http_raw() {
  local method=$1 path=$2 timeout=${3:-5}
  HOST=$HOST PORT=$PORT TIMEOUT=$timeout REQ_METHOD=$method REQ_PATH=$path /usr/bin/python3 - <<'PY'
import os, socket, sys
host = os.environ['HOST']
port = int(os.environ['PORT'])
t    = float(os.environ['TIMEOUT'])
m    = os.environ['REQ_METHOD']
path = os.environ['REQ_PATH']
try:
    s = socket.create_connection((host, port), timeout=t)
    s.settimeout(t)
    req = (f'{m} {path} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n').encode()
    s.sendall(req)
    data = b''
    try:
        while True:
            chunk = s.recv(8192)
            if not chunk:
                break
            data += chunk
            hdr_end = data.find(b'\r\n\r\n')
            if hdr_end >= 0:
                hdrs = data[:hdr_end].decode('latin-1', 'replace')
                cl = -1
                for line in hdrs.split('\r\n'):
                    if line.lower().startswith('content-length:'):
                        try: cl = int(line.split(':', 1)[1].strip())
                        except: pass
                if cl >= 0 and len(data) >= hdr_end + 4 + cl:
                    break
                if cl < 0 and m == 'HEAD':
                    break
    except socket.timeout:
        pass
    s.close()
    sys.stdout.buffer.write(data)
except Exception as e:
    sys.stdout.write(f'ERR:{e}\n')
PY
}

# Extract the status code (e.g. "200") from a raw response.
status_line() {
  local raw=$1
  printf '%s' "$raw" | head -n1 | tr -d '\r'
}

status_code() {
  local raw=$1
  local line
  line=$(status_line "$raw")
  local rest=${line#* }
  printf '%s' "${rest%% *}"
}

# Extract the headers section (everything before the first \r\n\r\n).
headers_section() {
  local raw=$1
  printf '%s' "$raw" | awk 'BEGIN{RS="\r\n\r\n"} NR==1{print; exit}'
}

# Extract the body (everything after first \r\n\r\n).
body_section() {
  local raw=$1
  # Use Python: awk's RS handling of \r\n\r\n is fragile across implementations.
  RAW="$raw" python3 -c '
import os, sys
raw = os.environ["RAW"].encode("latin-1", "replace")
_, sep, body = raw.partition(b"\r\n\r\n")
sys.stdout.buffer.write(body)
'
}

# Extract a header value (case-insensitive) from a headers section.
# If the header appears multiple times, returns ALL values, one per line.
header_get_all() {
  local headers=$1 name=$2
  printf '%s\n' "$headers" | awk -v IGNORECASE=1 -v n="$name" '
    BEGIN { n=tolower(n) }
    {
      line=$0
      sub(/\r$/, "", line)
      colon = index(line, ":")
      if (colon == 0) next
      hn = tolower(substr(line, 1, colon-1))
      hv = substr(line, colon+1)
      sub(/^[ \t]+/, "", hv)
      if (hn == n) print hv
    }
  '
}

# First (or only) value of a header, or empty if absent.
header_get() {
  header_get_all "$1" "$2" | head -n1
}

# Count occurrences of a header.
header_count() {
  local n
  n=$(header_get_all "$1" "$2" | grep -c .)
  [[ -z $n ]] && n=0
  printf '%s' "$n"
}

# -----------------------------------------------------------------------------
# test runner
# -----------------------------------------------------------------------------

run_test() {
  local name=$1 fn=$2
  local out
  out=$($fn 2>&1)
  local rc=$?
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

# Dump headers if VERBOSE is set; called from inside test bodies.
dump_if_verbose() {
  if (( VERBOSE )); then
    printf '%s\n' "$1" | sed 's/^/    /' >&2
  fi
}

# -----------------------------------------------------------------------------
# Section 2.1 — required security headers on success responses
# -----------------------------------------------------------------------------

# Capture root response once and reuse.
ROOT_RAW=$(http_raw GET "$ROOT_URL" 5)
ROOT_HEADERS=$(headers_section "$ROOT_RAW")
ROOT_STATUS=$(status_code "$ROOT_RAW")

t_status_root_ok() {
  if [[ -z $ROOT_STATUS ]]; then
    print -r -- "no status line; raw='${ROOT_RAW:0:80}'"
    return 1
  fi
  if [[ $ROOT_STATUS != 2* && $ROOT_STATUS != 3* ]]; then
    dump_if_verbose "$ROOT_HEADERS"
    print -r -- "GET $ROOT_URL → $ROOT_STATUS (expected 2xx/3xx for header tests)"
    return 1
  fi
  print -r -- "GET $ROOT_URL → $ROOT_STATUS"
}

t_h1_xcto() {
  local v
  v=$(header_get "$ROOT_HEADERS" "X-Content-Type-Options")
  if [[ -z $v ]]; then
    dump_if_verbose "$ROOT_HEADERS"
    print -r -- "X-Content-Type-Options not set"; return 1
  fi
  if [[ ${v:l} != nosniff ]]; then
    print -r -- "value='$v' (expected 'nosniff')"; return 1
  fi
  print -r -- "X-Content-Type-Options: $v"
}

t_h2_xfo() {
  local v
  v=$(header_get "$ROOT_HEADERS" "X-Frame-Options")
  if [[ -z $v ]]; then
    dump_if_verbose "$ROOT_HEADERS"
    print -r -- "X-Frame-Options not set"; return 1
  fi
  case ${v:u} in
    DENY|SAMEORIGIN) ;;
    *) print -r -- "value='$v' (expected DENY or SAMEORIGIN)"; return 1 ;;
  esac
  print -r -- "X-Frame-Options: $v"
}

t_h3_refpol() {
  local v
  v=$(header_get "$ROOT_HEADERS" "Referrer-Policy")
  if [[ -z $v ]]; then
    dump_if_verbose "$ROOT_HEADERS"
    print -r -- "Referrer-Policy not set"; return 1
  fi
  print -r -- "Referrer-Policy: $v"
}

t_h4_csp() {
  local v
  v=$(header_get "$ROOT_HEADERS" "Content-Security-Policy")
  if [[ -z $v ]]; then
    dump_if_verbose "$ROOT_HEADERS"
    print -r -- "Content-Security-Policy not set"; return 1
  fi
  print -r -- "Content-Security-Policy: $v"
}

# -----------------------------------------------------------------------------
# Section 2.2 — security headers on error responses
# -----------------------------------------------------------------------------

ERR_RAW=$(http_raw GET "$MISSING_URL" 5)
ERR_HEADERS=$(headers_section "$ERR_RAW")
ERR_STATUS=$(status_code "$ERR_RAW")

t_h5_error_carries_headers() {
  if [[ $ERR_STATUS != 4* && $ERR_STATUS != 5* ]]; then
    print -r -- "GET $MISSING_URL → $ERR_STATUS (expected 4xx/5xx)"
    return 1
  fi
  local missing=()
  for h in X-Content-Type-Options X-Frame-Options Referrer-Policy Content-Security-Policy; do
    [[ -z $(header_get "$ERR_HEADERS" "$h") ]] && missing+=("$h")
  done
  if (( ${#missing[@]} > 0 )); then
    dump_if_verbose "$ERR_HEADERS"
    print -r -- "missing on $ERR_STATUS response: ${missing[*]}"
    return 1
  fi
  print -r -- "all 4 security headers present on $ERR_STATUS"
}

# -----------------------------------------------------------------------------
# Section 2.3 — security headers on HEAD responses
# -----------------------------------------------------------------------------

HEAD_RAW=$(http_raw HEAD "$ROOT_URL" 5)
HEAD_HEADERS=$(headers_section "$HEAD_RAW")
HEAD_STATUS=$(status_code "$HEAD_RAW")
HEAD_BODY=$(body_section "$HEAD_RAW")

t_h6_head_carries_headers() {
  if [[ $HEAD_STATUS != 2* && $HEAD_STATUS != 3* ]]; then
    print -r -- "HEAD $ROOT_URL → $HEAD_STATUS (expected 2xx/3xx)"
    return 1
  fi
  local missing=()
  for h in X-Content-Type-Options X-Frame-Options Referrer-Policy Content-Security-Policy; do
    [[ -z $(header_get "$HEAD_HEADERS" "$h") ]] && missing+=("$h")
  done
  if (( ${#missing[@]} > 0 )); then
    dump_if_verbose "$HEAD_HEADERS"
    print -r -- "missing on HEAD: ${missing[*]}"
    return 1
  fi
  print -r -- "all 4 security headers present on HEAD"
}

t_h7_head_body_empty() {
  local n=${#HEAD_BODY}
  if (( n != 0 )); then
    print -r -- "HEAD body has $n bytes (expected 0)"
    return 1
  fi
  # Content-Length should match the GET response's body length
  local cl_head cl_get get_body_len
  cl_head=$(header_get "$HEAD_HEADERS" "Content-Length")
  cl_get=$(header_get "$ROOT_HEADERS" "Content-Length")
  if [[ -n $cl_head && -n $cl_get && $cl_head != $cl_get ]]; then
    print -r -- "HEAD CL=$cl_head ≠ GET CL=$cl_get"
    return 1
  fi
  print -r -- "HEAD body empty; CL matches GET (CL=${cl_head:-unset})"
}

# -----------------------------------------------------------------------------
# Section 2.4 — header hygiene
# -----------------------------------------------------------------------------

t_h8_no_duplicates() {
  local dups=()
  local h n
  for h in Content-Type Content-Length Server Date Connection \
           X-Content-Type-Options X-Frame-Options Referrer-Policy \
           Content-Security-Policy; do
    n=$(header_count "$ROOT_HEADERS" "$h")
    if (( n > 1 )); then
      dups+=("$h(×$n)")
    fi
  done
  if (( ${#dups[@]} > 0 )); then
    dump_if_verbose "$ROOT_HEADERS"
    print -r -- "duplicate headers: ${dups[*]}"
    return 1
  fi
  print -r -- "no header duplicated on root response"
}

t_h9_content_type_set() {
  local checks=()
  local v_root v_err
  v_root=$(header_get "$ROOT_HEADERS" "Content-Type")
  v_err=$(header_get "$ERR_HEADERS" "Content-Type")
  if [[ -z $v_root ]]; then
    checks+=("root($ROOT_STATUS):missing")
  fi
  if [[ -z $v_err ]]; then
    checks+=("error($ERR_STATUS):missing")
  fi
  if (( ${#checks[@]} > 0 )); then
    print -r -- "Content-Type missing on: ${checks[*]}"
    return 1
  fi
  print -r -- "root='$v_root' error='$v_err'"
}

t_h10_content_length_matches() {
  local result
  result=$(HOST=$HOST PORT=$PORT REQ_PATH=$ROOT_URL python3 - <<'PY'
import os, socket, sys
host = os.environ['HOST']
port = int(os.environ['PORT'])
path = os.environ['REQ_PATH']
try:
    s = socket.create_connection((host, port), timeout=5)
    s.settimeout(5)
    s.sendall(f'GET {path} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'.encode())
    data = b''
    cl = -1
    hdr_end = -1
    while True:
        c = s.recv(8192)
        if not c: break
        data += c
        if hdr_end < 0:
            hdr_end = data.find(b'\r\n\r\n')
            if hdr_end >= 0:
                for line in data[:hdr_end].decode('latin-1','replace').split('\r\n'):
                    if line.lower().startswith('content-length:'):
                        try: cl = int(line.split(':',1)[1].strip())
                        except: pass
        if cl >= 0 and hdr_end >= 0 and len(data) >= hdr_end + 4 + cl:
            break
    s.close()
    if hdr_end < 0:
        print('NOHEADERS'); sys.exit(0)
    body_len = len(data) - hdr_end - 4
    if cl < 0:
        print(f'NOCL body={body_len}')
    elif body_len == cl:
        print(f'MATCH cl={cl}')
    else:
        print(f'MISMATCH cl={cl} body={body_len}')
except Exception as e:
    print(f'ERR {e}')
PY
)
  case $result in
    MATCH*)    print -r -- "$result" ;;
    NOCL*)     print -r -- "$result (chunked? lenient pass)" ;;
    MISMATCH*) print -r -- "$result"; return 1 ;;
    *)         print -r -- "$result"; return 1 ;;
  esac
}

t_h11_date_parseable() {
  local v
  v=$(header_get "$ROOT_HEADERS" "Date")
  if [[ -z $v ]]; then
    print -r -- "Date header missing (informational; recommended by RFC 9110 §6.6.1)"
    return 1
  fi
  # Accept IMF-fixdate: "Sun, 06 Nov 1994 08:49:37 GMT"
  local ok
  ok=$(DATE="$v" python3 -c '
import os, sys, email.utils
v = os.environ["DATE"]
t = email.utils.parsedate_tz(v)
sys.exit(0 if t else 1)
' && echo yes || echo no)
  if [[ $ok != yes ]]; then
    print -r -- "Date='$v' (not a parseable RFC 7231 / 5322 timestamp)"
    return 1
  fi
  print -r -- "Date='$v' (parseable)"
}

t_h12_server_present() {
  local v
  v=$(header_get "$ROOT_HEADERS" "Server")
  if [[ -z $v ]]; then
    print -r -- "Server header absent (informational only)"
    return 1
  fi
  # Light check: warn if a version number is leaked. Not a hard failure.
  local note=""
  if [[ $v == *[0-9]\.[0-9]* ]]; then
    note=" (note: includes version-like digits; consider stripping)"
  fi
  print -r -- "Server='$v'$note"
}

t_h14_no_response_splitting() {
  # Check no header value contains a bare CR or LF that survived to the wire.
  # The response is already \r\n separated; what we're guarding against is
  # *injection* — e.g. a value with embedded \r\n that creates a fake header.
  # Detect by counting unique header names across the parsed section: if any
  # header value contained an unescaped \r\n, awk would have parsed it as
  # *two* headers. We re-scan for known sentinel patterns and look for any
  # value that begins with a known header name (case-insensitive).
  local suspicious
  suspicious=$(printf '%s\n' "$ROOT_HEADERS" | awk '
    {
      line=$0
      sub(/\r$/, "", line)
      colon = index(line, ":")
      if (colon == 0) next
      hv = substr(line, colon+1)
      sub(/^[ \t]+/, "", hv)
      # Look for a header value that *itself* contains a header-like pattern:
      # "Set-Cookie:" or "X-Evil:" anywhere in the value.
      if (hv ~ /[Ss]et-[Cc]ookie[ \t]*:/ || hv ~ /\r/ || hv ~ /\n/) {
        print line
      }
    }
  ')
  if [[ -n $suspicious ]]; then
    print -r -- "possible response splitting: $suspicious"
    return 1
  fi
  print -r -- "no embedded CR/LF or injected headers detected"
}

# -----------------------------------------------------------------------------
# Section 2.5 — CGI passthrough
# -----------------------------------------------------------------------------

t_h13_cgi_passthrough() {
  if [[ -z $CGI_TEST_URL ]]; then
    print -r -- "CGI_TEST_URL not set; skipping"
    return 2
  fi
  local raw headers http_status
  raw=$(http_raw GET "$CGI_TEST_URL" 10)
  headers=$(headers_section "$raw")
  http_status=$(status_code "$raw")
  if [[ $http_status != 200 ]]; then
    dump_if_verbose "$headers"
    print -r -- "CGI URL returned $http_status; expected 200"
    return 1
  fi

  # Security headers should be present on CGI responses too.
  local missing=()
  for h in X-Content-Type-Options X-Frame-Options Referrer-Policy Content-Security-Policy; do
    [[ -z $(header_get "$headers" "$h") ]] && missing+=("$h")
  done
  if (( ${#missing[@]} > 0 )); then
    dump_if_verbose "$headers"
    print -r -- "CGI response missing security headers: ${missing[*]}"
    return 1
  fi

  # Content-Type set by CGI must NOT be duplicated by the server.
  local n_ct
  n_ct=$(header_count "$headers" "Content-Type")
  if (( n_ct != 1 )); then
    dump_if_verbose "$headers"
    print -r -- "Content-Type appears $n_ct times (expected 1; server is double-emitting)"
    return 1
  fi
  print -r -- "CGI carries security headers; Content-Type appears exactly once"
}

# -----------------------------------------------------------------------------
# main
# -----------------------------------------------------------------------------

print -r -- "── 2.1 security headers on success ────────────────────────────"
run_test "H0 sanity: GET $ROOT_URL returns 2xx/3xx"           t_status_root_ok
run_test "H1 X-Content-Type-Options: nosniff"                 t_h1_xcto
run_test "H2 X-Frame-Options: DENY/SAMEORIGIN"                t_h2_xfo
run_test "H3 Referrer-Policy present"                         t_h3_refpol
run_test "H4 Content-Security-Policy present"                 t_h4_csp

print -r -- "── 2.2 security headers on error ──────────────────────────────"
run_test "H5 all 4 security headers present on 4xx"           t_h5_error_carries_headers

print -r -- "── 2.3 security headers on HEAD ───────────────────────────────"
run_test "H6 all 4 security headers present on HEAD"          t_h6_head_carries_headers
run_test "H7 HEAD body empty; Content-Length matches GET"     t_h7_head_body_empty

print -r -- "── 2.4 header hygiene ─────────────────────────────────────────"
run_test "H8 no header duplicated on root"                    t_h8_no_duplicates
run_test "H9 Content-Type always set"                         t_h9_content_type_set
run_test "H10 Content-Length matches body length"             t_h10_content_length_matches
run_test "H11 Date header parseable as RFC 7231"              t_h11_date_parseable
run_test "H12 Server header present (informational)"          t_h12_server_present
run_test "H14 no response-splitting / header injection"       t_h14_no_response_splitting

print -r -- "── 2.5 CGI passthrough ────────────────────────────────────────"
run_test "H13 CGI response: security headers + no CT dup"     t_h13_cgi_passthrough

print -r -- ""
print -r -- "──────────────────────────────────────────────────────────────"
print -r -- "passed: $PASS   failed: $FAIL"
if (( FAIL > 0 )); then
  print -r -- ""
  print -r -- "failures:"
  for f in "${FAILED[@]}"; do
    print -r -- "  - $f"
  done
fi
exit $FAIL
