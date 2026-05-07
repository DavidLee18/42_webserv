#!/usr/bin/env zsh
# webserv_chunked_tests.zsh — B7 Transfer-Encoding: chunked decoding tests.
#
# Probes the live server with chunked POST requests and inspects:
#   (a) HTTP status codes for malformed framing
#   (b) decoded body integrity for well-formed framing (echoed via CGI)
#
# Prerequisites:
#   - webserv running on $HOST:$PORT
#   - a POST-accepting upload route at $UPLOAD_URL (defaults to /upload)
#   - optionally, a CGI echo endpoint at $ECHO_CGI_URL that returns the
#     received body verbatim. If unset, body-integrity tests are skipped.
#
# Usage:
#   ./webserv_chunked_tests.zsh
#   HOST=127.0.0.1 PORT=8080 UPLOAD_URL=/upload ECHO_CGI_URL=/cgi/echo.cgi \
#     ./webserv_chunked_tests.zsh
#   VERBOSE=1 ./webserv_chunked_tests.zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
UPLOAD_URL=${UPLOAD_URL:-/upload}
ECHO_CGI_URL=${ECHO_CGI_URL:-}
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

print -- "target:    http://${HOST}:${PORT}"
print -- "upload:    $UPLOAD_URL"
print -- "echo-cgi:  ${ECHO_CGI_URL:-<not configured; integrity tests will be skipped>}"
print -- "(VERBOSE=1 to dump raw responses on failure)"
print -- ""

# -----------------------------------------------------------------------------
# Auto-deploy the echo CGI script if ECHO_CGI_URL is set.
# Adjust ECHO_CGI_PATH to match your server's CGI document root.
# -----------------------------------------------------------------------------
ECHO_CGI_PATH=${ECHO_CGI_PATH:-./spool/www/cgi-bin/echo.cgi}

if [[ -n $ECHO_CGI_URL ]]; then
  mkdir -p "${ECHO_CGI_PATH:h}"
  cat > "$ECHO_CGI_PATH" <<'CGI'
#!/usr/bin/env python3
import sys
sys.stdout.write("Content-Type: application/octet-stream\r\n\r\n")
sys.stdout.flush()
sys.stdout.buffer.write(sys.stdin.buffer.read())
CGI
  chmod +x "$ECHO_CGI_PATH"
  print -- "deployed: $ECHO_CGI_PATH"
  print -- ""
fi

# -----------------------------------------------------------------------------
# Send a raw chunked POST and capture the response.
#   $1 path
#   $2 raw body (already chunk-framed by the caller; literal \r\n bytes)
#   prints: HTTP status code on stdout, full response on stderr
# -----------------------------------------------------------------------------
send_chunked() {
  local path=$1 body=$2  
  local body_b64
  body_b64=$(printf -- '%s' "$body" | "$PYTHON" -c 'import sys,base64; sys.stdout.write(base64.b64encode(sys.stdin.buffer.read()).decode())')
  "$PYTHON" - "$HOST" "$PORT" "$path" "$body_b64" <<'PY' 2>/dev/null
import socket, sys
import base64
host, port, path, body_b64 = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
body = base64.b64decode(body_b64)
req = (
    f"POST {path} HTTP/1.1\r\n"
    f"Host: {host}\r\n"
    f"Transfer-Encoding: chunked\r\n"
    f"Content-Type: application/octet-stream\r\n"
    f"Connection: close\r\n"
    f"\r\n"
).encode("latin-1") + body
try:
    s = socket.create_connection((host, port), timeout=5)
    s.sendall(req)
    data = b""
    while True:
        chunk = s.recv(8192)
        if not chunk: break
        data += chunk
    s.close()
except Exception as e:
    print(f"---", end="")
    sys.stderr.write(f"send error: {e}\n")
    sys.exit(0)

# Status code → stdout; full response → stderr for VERBOSE dumping.
line = data.split(b"\r\n", 1)[0].decode("latin-1", "replace")
parts = line.split(" ")
status = parts[1] if len(parts) >= 2 and parts[0].startswith("HTTP/") else "---"
print(status, end="")
sys.stderr.buffer.write(data)
PY
}

# Run a status-only test.
#   $1 name  $2 chunked-body  $3 expected-status-regex
status_test() {
  local name=$1 body=$2 expect=$3
  local out err
  err=$(mktemp)
  out=$(send_chunked "$UPLOAD_URL" "$body" 2>"$err")
  if [[ $out =~ $expect ]]; then
    print -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— status $out${C_OFF}"
    (( PASS++ ))
  else
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— status $out (expected $expect)${C_OFF}"
    if (( VERBOSE )); then
      print -- "${C_DIM}  raw response:${C_OFF}"
      sed 's/^/    /' "$err" >&2
    fi
    FAILED+=("$name")
    (( FAIL++ ))
  fi
  rm -f "$err"
}

# Run a body-integrity test (requires ECHO_CGI_URL).
#   $1 name  $2 chunked-body  $3 expected-decoded-body
integrity_test() {
  local name=$1 body=$2 expected=$3
  if [[ -z $ECHO_CGI_URL ]]; then
    print -- "${C_DIM}SKIP $name — ECHO_CGI_URL not set${C_OFF}"
    return
  fi
  local err
  err=$(mktemp)
  send_chunked "$ECHO_CGI_URL" "$body" 2>"$err" >/dev/null

  # Extract body (after first \r\n\r\n) from the captured response.
  local got
  got=$("$PYTHON" -c "
import sys
data = sys.stdin.buffer.read()
i = data.find(b'\r\n\r\n')
sys.stdout.buffer.write(data[i+4:] if i >= 0 else b'')
" < "$err")

  if [[ $got == $expected ]]; then
    print -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— body ${#got} bytes match${C_OFF}"
    (( PASS++ ))
  else
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— body mismatch (got ${#got} bytes, expected ${#expected} bytes)${C_OFF}"
    if (( VERBOSE )); then
      print -- "${C_DIM}  expected: $(print -r -- "$expected" | head -c 200)${C_OFF}"
      print -- "${C_DIM}  got:      $(print -r -- "$got" | head -c 200)${C_OFF}"
    fi
    FAILED+=("$name")
    (( FAIL++ ))
  fi
  rm -f "$err"
}

# -----------------------------------------------------------------------------
# Section A — status-only probes (malformed framing → 400/413; well-formed → 2xx)
# -----------------------------------------------------------------------------

print -- "── A. malformed chunked framing should be rejected ─────────────"

# A1: non-hex chunk size
status_test "A1 non-hex chunk size → 400" \
  $'GG\r\nhello\r\n0\r\n\r\n' \
  '^4[0-9][0-9]$'

# A2: missing CRLF after chunk data
status_test "A2 missing CRLF after data → 400" \
  $'5\r\nhelloMISSING0\r\n\r\n' \
  '^4[0-9][0-9]$'

# A3: chunk size declares more bytes than provided, then connection closes
status_test "A3 short chunk data → 400" \
  $'a\r\nhi\r\n0\r\n\r\n' \
  '^4[0-9][0-9]$'

# A4: missing terminator (no 0-chunk before EOF)
# NOTE: server may either 400 or hang-then-timeout; both acceptable.
status_test "A4 missing 0-chunk terminator → 4xx" \
  $'5\r\nhello\r\n' \
  '^(4[0-9][0-9]|408)$'

# A5: chunk size > 10 MiB cap (allocation bomb defence)
status_test "A5 oversized chunk (16 MiB) → 413/400" \
  $'1000000\r\n' \
  '^4[0-9][0-9]$'

# -----------------------------------------------------------------------------
# Section B — well-formed chunked framing should be accepted
# -----------------------------------------------------------------------------

print -- ""
print -- "── B. well-formed chunked framing should be accepted ───────────"

# B1: empty body (just the terminator)
status_test "B1 empty body (0\\r\\n\\r\\n) → 2xx/3xx" \
  $'0\r\n\r\n' \
  '^[23][0-9][0-9]$'

# B2: single chunk
status_test "B2 single chunk → 2xx" \
  $'5\r\nhello\r\n0\r\n\r\n' \
  '^[23][0-9][0-9]$'

# B3: multiple chunks
status_test "B3 multiple chunks → 2xx" \
  $'5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n' \
  '^[23][0-9][0-9]$'

# B4: chunk extension (must be ignored)
status_test "B4 chunk with extension → 2xx" \
  $'5;name=value\r\nhello\r\n0\r\n\r\n' \
  '^[23][0-9][0-9]$'

# B5: uppercase hex
status_test "B5 uppercase hex size → 2xx" \
  $'19\r\nThis is twenty-five bytes\r\n0\r\n\r\n' \
  '^[23][0-9][0-9]$'

# B6: chunk data containing \r\n (must not split on it)
status_test "B6 chunk data containing CRLF → 2xx" \
  $'8\r\nfoo\r\nbar\r\n0\r\n\r\n' \
  '^[23][0-9][0-9]$'

# B7: trailers after 0-chunk (usually empty; some clients send headers)
status_test "B7 trailers after terminator → 2xx" \
  $'5\r\nhello\r\n0\r\nX-Trailer: yes\r\n\r\n' \
  '^[23][0-9][0-9]$'

# -----------------------------------------------------------------------------
# Section C — body integrity (requires CGI echo endpoint)
# -----------------------------------------------------------------------------

print -- ""
print -- "── C. decoded body integrity (CGI echo) ────────────────────────"

# C1: simple decoded body
integrity_test "C1 hello world decode" \
  $'5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n' \
  "hello world"

# C2: chunk extensions stripped from decoded output
integrity_test "C2 chunk extensions stripped" \
  $'5;ext=foo\r\nhello\r\n0\r\n\r\n' \
  "hello"

# C3: data with embedded CRLF preserved
integrity_test "C3 embedded CRLF in data preserved" \
  $'8\r\nfoo\r\nbar\r\n0\r\n\r\n' \
  $'foo\r\nbar'

# C4: empty body
integrity_test "C4 empty body" \
  $'0\r\n\r\n' \
  ""

# C5: 1 KiB body across many small chunks (stress the loop)
{
  local big_body="" big_chunked=""
  local chunk
  for i in {1..64}; do
    chunk=$(printf '%016d' $i)   # 16 bytes per chunk
    big_body+="$chunk"
    big_chunked+=$'10\r\n'"$chunk"$'\r\n'
  done
  big_chunked+=$'0\r\n\r\n'
  integrity_test "C5 64×16-byte chunks (1 KiB total)" \
    "$big_chunked" \
    "$big_body"
}

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------

print -- ""
print -- "──────────────────────────────────────────────────────────────"
print -- "passed: $PASS   failed: $FAIL"
if (( FAIL > 0 )); then
  print -- ""
  print -- "failures:"
  for f in "${FAILED[@]}"; do
    print -- "  - $f"
  done
fi

exit $FAIL
