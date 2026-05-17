#!/usr/bin/env zsh
# webserv_eval_smoke_tests.zsh — eval-sheet smoke probes for features the
# parsing/exhaustion/cgi/chunked suites don't directly exercise.
#
# Sections:
#   SM1  DELETE /storage/<file> round-trip after upload  (DELETE, upload+retrieve)
#   SM2  DELETE without auth → 4xx                       (method gating)
#   SM3  GET /old_stuff/ → 301 + Location                (HTTP redirection)
#   SM4  GET /download/ → 200 + autoindex HTML           (directory listing)
#
# Usage:
#   ./webserv_eval_smoke_tests.zsh
#   HOST=127.0.0.1 PORT=8080 VERBOSE=1 ./webserv_eval_smoke_tests.zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
LOGIN_URL=${LOGIN_URL:-/login}
LOGIN_BODY=${LOGIN_BODY:-id=david&pw=david1234}
STORAGE_URL=${STORAGE_URL:-/storage/}
REDIRECT_URL=${REDIRECT_URL:-/old_stuff/}
REDIRECT_EXPECTED_LOCATION=${REDIRECT_EXPECTED_LOCATION:-/new_stuff/}
AUTOINDEX_URL=${AUTOINDEX_URL:-/download/}
VERBOSE=${VERBOSE:-0}
PYTHON=${PYTHON:-$(command -v python3)}

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_SKIP=$'\e[33m'
  C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_SKIP=''; C_DIM=''; C_OFF=''
fi

PASS=0; FAIL=0; SKIP=0
typeset -a FAILED
FAILED=()

print -- "target:    http://${HOST}:${PORT}"
print -- "(VERBOSE=1 to dump raw responses on failure)"
print -- ""

# -----------------------------------------------------------------------------
# helpers
# -----------------------------------------------------------------------------

run_test() {
  local name=$1 fn=$2 out rc
  out=$($fn 2>&1); rc=$?
  case $rc in
    0)
      print -r -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— $out${C_OFF}"
      (( PASS++ ))
      ;;
    2)
      print -r -- "${C_SKIP}SKIP${C_OFF} $name ${C_DIM}— $out${C_OFF}"
      (( SKIP++ ))
      ;;
    *)
      print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— $out${C_OFF}"
      FAILED+=("$name")
      (( FAIL++ ))
      ;;
  esac
}

# Try to log in; sets SESSION_COOKIE on success, leaves empty on failure.
SESSION_COOKIE=""
login_once() {
  SESSION_COOKIE=$(HOST=$HOST PORT=$PORT LOGIN=$LOGIN_URL BODY=$LOGIN_BODY \
    "$PYTHON" - <<'PY'
import os, socket
host=os.environ['HOST']; port=int(os.environ['PORT'])
path=os.environ['LOGIN']; body=os.environ['BODY'].encode('latin-1')
req=(f"POST {path} HTTP/1.1\r\nHost: {host}\r\n"
     f"Content-Type: application/x-www-form-urlencoded\r\n"
     f"Content-Length: {len(body)}\r\nConnection: close\r\n\r\n").encode()+body
data=b""
try:
    s=socket.create_connection((host,port),timeout=5); s.sendall(req)
    while True:
        try: c=s.recv(8192)
        except Exception: break
        if not c: break
        data+=c
    s.close()
except Exception:
    pass
for line in data.split(b"\r\n"):
    if line.lower().startswith(b"set-cookie:"):
        ck=line[len(b"set-cookie:"):].strip()
        if b";" in ck: ck=ck.split(b";",1)[0]
        print(ck.decode("latin-1","replace"))
        break
PY
  )
  if [[ -n $SESSION_COOKIE ]]; then
    print -- "${C_DIM}logged in: ${SESSION_COOKIE}${C_OFF}"
  else
    print -- "${C_DIM}login failed; SM1 will be skipped${C_OFF}"
  fi
  print -- ""
}

# -----------------------------------------------------------------------------
# SM1 — DELETE /storage/<file> round-trip after upload.
# Uploads a small multipart file, verifies presence, deletes, verifies absence.
# -----------------------------------------------------------------------------
t_sm1_delete_roundtrip() {
  if [[ -z $SESSION_COOKIE ]]; then
    print -r -- "no session cookie (login failed)"
    return 2
  fi
  HOST=$HOST PORT=$PORT STORE=$STORAGE_URL COOKIE=$SESSION_COOKIE "$PYTHON" - <<'PY'
import os, socket, sys, time
host=os.environ['HOST']; port=int(os.environ['PORT'])
store=os.environ['STORE']; cookie=os.environ['COOKIE']
fname=f"smoke_{os.getpid()}_{int(time.time())}.txt"
bnd="----smokeBoundary42"
contents=b"smoke-test-payload\n"
mp=(f"--{bnd}\r\n"
    f'Content-Disposition: form-data; name=\"file\"; filename=\"{fname}\"\r\n'
    f"Content-Type: text/plain\r\n\r\n").encode()+contents+f"\r\n--{bnd}--\r\n".encode()
def send(req):
    s=socket.create_connection((host,port),timeout=10); s.settimeout(10)
    s.sendall(req); data=b""
    while True:
        try: c=s.recv(65536)
        except Exception: break
        if not c: break
        data+=c
    s.close(); return data
def code_of(data):
    line=data.split(b"\r\n",1)[0].decode('latin-1','replace') if data else ''
    return line.split(' ')[1] if line.startswith('HTTP/') else '---'
up_req=(f"POST {store} HTTP/1.1\r\nHost: {host}\r\nCookie: {cookie}\r\n"
        f"Content-Type: multipart/form-data; boundary={bnd}\r\n"
        f"Content-Length: {len(mp)}\r\nConnection: close\r\n\r\n").encode()+mp
up=code_of(send(up_req))
if not up.startswith('2'):
    sys.stderr.write(f"upload failed: code={up}\n"); sys.exit(1)
get_req=(f"GET {store}{fname} HTTP/1.1\r\nHost: {host}\r\nCookie: {cookie}\r\n"
         f"Connection: close\r\n\r\n").encode()
gt=code_of(send(get_req))
if not gt.startswith('2'):
    sys.stderr.write(f"post-upload GET failed: code={gt}\n"); sys.exit(1)
del_req=(f"DELETE {store}{fname} HTTP/1.1\r\nHost: {host}\r\nCookie: {cookie}\r\n"
         f"Connection: close\r\n\r\n").encode()
dl=code_of(send(del_req))
if not dl.startswith('2'):
    sys.stderr.write(f"DELETE failed: code={dl}\n"); sys.exit(1)
gone=code_of(send(get_req))
if not gone.startswith('4'):
    sys.stderr.write(f"post-DELETE GET expected 4xx, got {gone}\n"); sys.exit(1)
sys.stdout.write(f"upload={up}, get={gt}, del={dl}, post-del-get={gone}")
PY
}

# -----------------------------------------------------------------------------
# SM2 — DELETE without session cookie → must be rejected (4xx).
# -----------------------------------------------------------------------------
t_sm2_delete_unauth() {
  HOST=$HOST PORT=$PORT STORE=$STORAGE_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT']); store=os.environ['STORE']
req=(f"DELETE {store}nonexistent_smoke_target HTTP/1.1\r\nHost: {host}\r\n"
     f"Connection: close\r\n\r\n").encode()
data=b""
try:
    s=socket.create_connection((host,port),timeout=5); s.sendall(req)
    while True:
        try: c=s.recv(8192)
        except Exception: break
        if not c: break
        data+=c
    s.close()
except Exception as e:
    sys.stderr.write(f"err: {e}\n"); sys.exit(1)
line=data.split(b"\r\n",1)[0].decode('latin-1','replace') if data else ''
code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
if not code.startswith('4'):
    sys.stderr.write(f"unauthenticated DELETE got {code} (expected 4xx)\n"); sys.exit(1)
sys.stdout.write(f"code={code}")
PY
}

# -----------------------------------------------------------------------------
# SM3 — GET /old_stuff/ → 301 with matching Location header.
# -----------------------------------------------------------------------------
t_sm3_redirect() {
  HOST=$HOST PORT=$PORT URL=$REDIRECT_URL EXP=$REDIRECT_EXPECTED_LOCATION "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT'])
url=os.environ['URL']; expected=os.environ['EXP']
req=(f"GET {url} HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n").encode()
data=b""
try:
    s=socket.create_connection((host,port),timeout=5); s.sendall(req)
    while True:
        try: c=s.recv(8192)
        except Exception: break
        if not c: break
        data+=c
    s.close()
except Exception as e:
    sys.stderr.write(f"err: {e}\n"); sys.exit(1)
hdr=data.split(b"\r\n\r\n",1)[0]
lines=hdr.split(b"\r\n")
status=lines[0].decode('latin-1','replace') if lines else ''
code=status.split(' ')[1] if status.startswith('HTTP/') else '---'
loc=None
for ln in lines[1:]:
    if ln.lower().startswith(b'location:'):
        loc=ln[len(b'location:'):].strip().decode('latin-1','replace'); break
if code != '301':
    sys.stderr.write(f"expected 301, got {code}\n"); sys.exit(1)
if loc is None:
    sys.stderr.write(f"301 without Location header\n"); sys.exit(1)
if loc != expected:
    sys.stderr.write(f"Location={loc} (expected {expected})\n"); sys.exit(1)
sys.stdout.write(f"code={code} Location={loc}")
PY
}

# -----------------------------------------------------------------------------
# SM4 — GET /download/ → 200 + HTML body (autoindex/directory listing).
# -----------------------------------------------------------------------------
t_sm4_autoindex() {
  HOST=$HOST PORT=$PORT URL=$AUTOINDEX_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT']); url=os.environ['URL']
req=(f"GET {url} HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n").encode()
data=b""
try:
    s=socket.create_connection((host,port),timeout=5); s.sendall(req)
    while True:
        try: c=s.recv(65536)
        except Exception: break
        if not c: break
        data+=c
    s.close()
except Exception as e:
    sys.stderr.write(f"err: {e}\n"); sys.exit(1)
hdr,_,body=data.partition(b"\r\n\r\n")
status=hdr.split(b"\r\n",1)[0].decode('latin-1','replace') if hdr else ''
code=status.split(' ')[1] if status.startswith('HTTP/') else '---'
if code != '200':
    sys.stderr.write(f"expected 200, got {code}\n"); sys.exit(1)
low=body.lower()
if b'<html' not in low and b'<!doctype html' not in low:
    sys.stderr.write(f"body not HTML (first 80B: {body[:80]!r})\n"); sys.exit(1)
sys.stdout.write(f"code={code} body={len(body)}B, HTML detected")
PY
}

# -----------------------------------------------------------------------------
# Runner
# -----------------------------------------------------------------------------

print -- "── eval-sheet smoke probes ────────────────────────────────────"
login_once
run_test "SM1 DELETE /storage/<file> round-trip"     t_sm1_delete_roundtrip
run_test "SM2 DELETE without auth → 4xx"             t_sm2_delete_unauth
run_test "SM3 GET /old_stuff/ → 301 + Location"      t_sm3_redirect
run_test "SM4 GET /download/ → autoindex HTML"       t_sm4_autoindex

print -- ""
print -- "──────────────────────────────────────────────────────────────"
print -- "passed: $PASS   failed: $FAIL   skipped: $SKIP"
if (( FAIL > 0 )); then
  print -- ""
  print -- "failures:"
  for f in "${FAILED[@]}"; do
    print -- "  - $f"
  done
fi
exit $FAIL
