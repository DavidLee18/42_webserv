#!/usr/bin/env zsh
# webserv_parsing_tests.zsh -- request-parsing hardening test suite
#
# Probes a webserv for the failure modes that actually get servers killed:
# request smuggling, header injection, path traversal, URL-decode ordering,
# method/version handling, oversized inputs.
#
# Usage:   ./webserv_parsing_tests.zsh
#          HOST=127.0.0.1 PORT=8080 ./webserv_parsing_tests.zsh
#          VERBOSE=1 ./webserv_parsing_tests.zsh    # dump req/resp on FAIL
#
# Exit code: 0 if every test passes, 1 otherwise.
#
# Each test has an RFC reference. Where the RFC permits more than one
# acceptable status, the regex covers the legitimate set.
# A FAIL is "your server diverged from the spec or, worse, did something
# unsafe" -- not "your server crashed", which the harness reports separately.

emulate -L zsh
setopt pipe_fail

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
VERBOSE=${VERBOSE:-0}

# ---------- colour helpers (degrade gracefully if not a TTY) ---------------
if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

typeset -i PASS=0 FAIL=0 SKIP=0
typeset -a FAILED

# ---------- raw TCP send/recv via embedded python3 -------------------------
# stdin = raw request bytes; stdout = raw response bytes.
# Connect timeout 2 s, idle-read timeout 2 s. Empty stdout means the server
# closed without responding (also a finding -- reported as status "---").
#
# NOTE: we pass the python source via -c (NOT a heredoc), because a heredoc
# would occupy python's stdin and the actual request bytes from our pipe
# would never reach the helper. Classic shell trap.
send_raw() {
  python3 -c '
import socket, sys
host, port = sys.argv[1], int(sys.argv[2])
data = sys.stdin.buffer.read()
s = socket.socket()
s.settimeout(2.0)
try:
    s.connect((host, port))
except Exception as e:
    sys.stderr.write("CONNECT_FAIL: " + str(e) + "\n"); sys.exit(2)
try:
    s.sendall(data)
    try: s.shutdown(socket.SHUT_WR)
    except OSError: pass
    buf = b""
    while True:
        try: chunk = s.recv(8192)
        except socket.timeout: break
        if not chunk: break
        buf += chunk
        if len(buf) > 1 << 20: break
    sys.stdout.buffer.write(buf)
except Exception as e:
    sys.stderr.write("IO_FAIL: " + str(e) + "\n")
finally:
    s.close()
' "$HOST" "$PORT"
}

# ---------- preflight: is the server up? -----------------------------------
preflight() {
  if ! python3 -c '
import socket, sys
s = socket.socket(); s.settimeout(1.5)
try: s.connect((sys.argv[1], int(sys.argv[2])))
except Exception: sys.exit(1)
' "$HOST" "$PORT" 2>/dev/null; then
    print -u2 -- "${C_FAIL}cannot connect to $HOST:$PORT${C_OFF}"
    print -u2 -- "start your webserv first, or set HOST=/PORT= env vars."
    exit 2
  fi
}

# ---------- run a single test ----------------------------------------------
# $1 name   $2 expected-status regex   $3 raw request (with \r\n, \xHH escapes)
expect() {
  local name=$1 expected=$2 raw=$3
  local resp code line

  # IMPORTANT: pipe printf directly to send_raw. Capturing into a variable
  # via $(...) would strip trailing newlines (POSIX command-substitution
  # rule), corrupting any request that ends in \r\n\r\n -- which is most of
  # them. The response capture is fine: a leading status line is preserved.
  resp=$(printf -- "%b" "$raw" | send_raw 2>/dev/null) || true

  # Robust status extraction: find first line starting with "HTTP/".
  code="---"
  for line in ${(f)resp}; do
    if [[ $line == HTTP/* ]]; then
      code=${${(z)line}[2]:-???}
      break
    fi
  done

  if [[ $code =~ $expected ]]; then
    print -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}→ $code${C_OFF}"
    (( PASS++ ))
  else
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}→ got '$code', expected /$expected/${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    if (( VERBOSE )); then
      print -- "${C_DIM}--- request (escape form) ---${C_OFF}"
      print -r -- "$raw"
      print -- "${C_DIM}--- response (first 400 bytes) ---${C_OFF}"
      print -r -- "${resp[1,400]}"
      print -- "${C_DIM}---------------${C_OFF}"
    fi
  fi
}

# ---------- check that a 2xx response does NOT contain a string -----------
# Used for path-traversal: even if status is 200, the body must not leak
# /etc/passwd content. $1 name, $2 needle, $3 raw request.
expect_no_leak() {
  local name=$1 needle=$2 raw=$3 resp code line
  resp=$(printf -- "%b" "$raw" | send_raw 2>/dev/null) || true
  code="---"
  for line in ${(f)resp}; do
    if [[ $line == HTTP/* ]]; then
      code=${${(z)line}[2]:-???}
      break
    fi
  done
  if [[ $resp == *"$needle"* ]]; then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}→ leaked '$needle' (status $code)${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
  else
    print -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}→ no leak (status $code)${C_OFF}"
    (( PASS++ ))
  fi
}

# ===========================================================================
preflight
print -- "${C_DIM}target: http://$HOST:$PORT${C_OFF}"
print -- "${C_DIM}(set VERBOSE=1 for failure dumps)${C_OFF}"
print -- ""

# ---------------------------------------------------------------------------
print -- "── A. Sanity ──────────────────────────────────────────────────────"
# ---------------------------------------------------------------------------
expect "A1 baseline GET /" \
  '^(200|301|302|404)$' \
  'GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'

# ---------------------------------------------------------------------------
print -- "── B. Request smuggling (RFC 9112 §6.1, §6.3) ─────────────────────"
# ---------------------------------------------------------------------------
# The classic CL.TE / TE.CL split. RFC 9112 §6.1: if both Content-Length
# and Transfer-Encoding are present, the message MUST be rejected.
expect "B1 CL + TE:chunked together" \
  '^400$' \
  'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 6\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n'

# RFC 9112 §6.3: duplicate Content-Length values must be rejected.
expect "B2 duplicate Content-Length, same value" \
  '^400$' \
  'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nhello'

expect "B3 conflicting Content-Length values" \
  '^400$' \
  'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello'

expect "B4 comma-listed Content-Length" \
  '^400$' \
  'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5, 5\r\n\r\nhello'

expect "B5 negative Content-Length" \
  '^400$' \
  'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: -5\r\n\r\n'

expect "B6 non-numeric Content-Length" \
  '^400$' \
  'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: abc\r\n\r\n'

expect "B7 hex-prefixed Content-Length" \
  '^400$' \
  'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 0x10\r\n\r\n'

# RFC 9112 §6.1: server MUST reject requests with a Transfer-Encoding it
# doesn't understand (501) or treat as malformed (400).
expect "B8 unknown Transfer-Encoding coding" \
  '^(400|501)$' \
  'POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: bogus-coding\r\n\r\n'

# Chunked but with non-hex chunk size: must be 400.
expect "B9 chunked: non-hex chunk size" \
  '^(400|501)$' \
  'POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n'

# ---------------------------------------------------------------------------
print -- "── C. Header injection / framing (RFC 9110 §5.5, RFC 9112 §2.2) ──"
# ---------------------------------------------------------------------------
# Bare CR or LF inside a header field-value is forbidden -- this is the
# primary injection vector for CRLF-injection / response-splitting style
# attacks reflected back into the request parser.
expect "C1 bare CR in header value" \
  '^400$' \
  'GET / HTTP/1.1\r\nHost: x\r\nX-Bad: hello\rworld\r\n\r\n'

expect "C2 bare LF in header value" \
  '^400$' \
  'GET / HTTP/1.1\r\nHost: x\r\nX-Bad: hello\nworld\r\n\r\n'

# RFC 9110 §5.5: NUL is forbidden in field-value. Many servers truncate
# silently at NUL (which is the actual security bug).
expect "C3 NUL in header value" \
  '^400$' \
  'GET / HTTP/1.1\r\nHost: x\r\nX-Bad: foo\x00bar\r\n\r\n'

expect "C4 NUL in request-target" \
  '^400$' \
  'GET /\x00foo HTTP/1.1\r\nHost: x\r\n\r\n'

# obs-fold (RFC 9112 §5.2): leading whitespace on a continuation line was
# deprecated in 7230 and made forbidden in 9112 except in message/http.
expect "C5 obs-fold (deprecated) header" \
  '^400$' \
  'GET / HTTP/1.1\r\nHost: x\r\nX-Folded: hello\r\n world\r\n\r\n'

# Whitespace before colon: invalid per RFC 9112 §5.1.
expect "C6 whitespace before header colon" \
  '^400$' \
  'GET / HTTP/1.1\r\nHost: x\r\nX-Bad : v\r\n\r\n'

# Empty header name.
expect "C7 empty header name" \
  '^400$' \
  'GET / HTTP/1.1\r\nHost: x\r\n: value\r\n\r\n'

# Oversized single header line. Real-world limit ~8 KiB; we send 16 KiB.
typeset BIG_HDR
BIG_HDR=$(printf 'X%.0s' {1..16384})
expect "C8 oversized header line (16 KiB)" \
  '^(400|431|413|494|414)$' \
  "GET / HTTP/1.1\r\nHost: x\r\nX-Big: ${BIG_HDR}\r\n\r\n"

# Many headers (header-table flooding).
typeset MANY_HDRS=""
for i in {1..500}; do MANY_HDRS+="X-H-$i: v\r\n"; done
expect "C9 too many headers (500)" \
  '^(400|431)$' \
  "GET / HTTP/1.1\r\nHost: x\r\n${MANY_HDRS}\r\n"

# ---------------------------------------------------------------------------
print -- "── D. Path traversal & URL decoding ───────────────────────────────"
# ---------------------------------------------------------------------------
# The dangerous failure mode here is *content leak*, not status code. The
# RFC-correct behaviour is: normalise the path, reject or 404 if it escapes
# the document root. A 200 carrying root:x:0:0 is the smoking gun.

expect_no_leak "D1 literal ../ does not leak /etc/passwd" \
  'root:x:0:0' \
  'GET /../../../../etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n'

expect_no_leak "D2 nested ../ does not leak" \
  'root:x:0:0' \
  'GET /a/b/../../../../../etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n'

# %2e%2e/ is the URL-encoded ../ -- after one decode it MUST be normalised.
expect_no_leak "D3 percent-encoded ../ does not leak" \
  'root:x:0:0' \
  'GET /%2e%2e/%2e%2e/%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n'

# Mixed case in hex digits -- common WAF bypass.
expect_no_leak "D4 mixed-case %2E%2e/ does not leak" \
  'root:x:0:0' \
  'GET /%2E%2e/%2E%2e/%2E%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n'

# Double-encoded: %25 → '%' then %2e → '.'. A correct server decodes ONCE,
# so the path is the literal string '%2e%2e/etc/passwd', which should 404.
# A server that decodes twice (the bug) will leak.
expect_no_leak "D5 double-encoded %252e%252e/ does not leak (single-decode rule)" \
  'root:x:0:0' \
  'GET /%252e%252e/%252e%252e/%252e%252e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n'

# Encoded NUL: %00 -- C-string truncation in path handling is a classic.
expect "D6 encoded NUL %00 in path is rejected" \
  '^(400|404)$' \
  'GET /index.html%00.jpg HTTP/1.1\r\nHost: x\r\n\r\n'

# Backslash should not be normalised to forward slash (Windows-style trick).
expect_no_leak "D7 backslash not treated as path separator" \
  'root:x:0:0' \
  'GET /..\\..\\..\\..\\etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n'

# Path with raw space -- must be 400 (RFC 9112 §3.1.1: SP terminates target).
expect "D8 unencoded space in request-target" \
  '^400$' \
  'GET /foo bar HTTP/1.1\r\nHost: x\r\n\r\n'

# ---------------------------------------------------------------------------
print -- "── E. Method, version, request-line ───────────────────────────────"
# ---------------------------------------------------------------------------
# RFC 9110 §9.1: methods are case-sensitive. Lowercase 'get' must not match.
expect "E1 lowercase method 'get'" \
  '^(400|501)$' \
  'get / HTTP/1.1\r\nHost: x\r\n\r\n'

# Unknown method: 501 per RFC 9110 §15.6.2.
expect "E2 unknown method PROPFIND" \
  '^(400|501|405)$' \
  'PROPFIND / HTTP/1.1\r\nHost: x\r\n\r\n'

# RFC 9112 §3.2: HTTP/1.1 requests without Host MUST be rejected.
expect "E3 HTTP/1.1 without Host header" \
  '^400$' \
  'GET / HTTP/1.1\r\n\r\n'

# RFC 9112 §3.2: multiple Host headers also rejected.
expect "E4 multiple Host headers" \
  '^400$' \
  'GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n'

# Version handling.
expect "E5 unsupported HTTP version 9.9" \
  '^(400|505)$' \
  'GET / HTTP/9.9\r\nHost: x\r\n\r\n'

expect "E6 HTTP/2.0 over plain TCP not supported here" \
  '^(400|505|426)$' \
  'GET / HTTP/2.0\r\nHost: x\r\n\r\n'

expect "E7 malformed version 'HTTP/1' (no minor)" \
  '^(400|505)$' \
  'GET / HTTP/1\r\nHost: x\r\n\r\n'

# Oversized request line.
typeset LONG_PATH
LONG_PATH=$(printf '/aaaaaaaaa%.0s' {1..2000})
expect "E8 oversized request-line (~20 KiB)" \
  '^(400|414)$' \
  "GET ${LONG_PATH} HTTP/1.1\r\nHost: x\r\n\r\n"

# Empty/junk requests.
expect "E9 just CRLF on its own" \
  '^(400|---)$' \
  '\r\n'

expect "E10 garbage non-HTTP bytes" \
  '^(400|---)$' \
  '\xff\xfe\xfd\xfc not http\r\n\r\n'

# ---------------------------------------------------------------------------
print -- "── F. Resilience ──────────────────────────────────────────────────"
# ---------------------------------------------------------------------------
# After every malformed request above, the server should still answer a
# valid request. If F1 fails, the server got into a bad state.
expect "F1 server still healthy after the gauntlet" \
  '^(200|301|302|404)$' \
  'GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'

# ---------------------------------------------------------------------------
print -- ""
print -- "──────────────────────────────────────────────────────────────────"
print -- "passed: ${C_PASS}${PASS}${C_OFF}   failed: ${C_FAIL}${FAIL}${C_OFF}"
if (( FAIL > 0 )); then
  print -- ""
  print -- "${C_FAIL}failures:${C_OFF}"
  for f in $FAILED; do print -- "  - $f"; done
  exit 1
fi
exit 0
