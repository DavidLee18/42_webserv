#!/usr/bin/env zsh
# webserv_exhaustion_tests.zsh — Section 5.2: resource-exhaustion / size-limit
# enforcement. Server must reject oversized inputs cleanly (4xx) without
# crashing or pre-allocating absurd amounts of memory.
#
# Usage:
#   PORT=8080 ./webserv_exhaustion_tests.zsh
#   MAX_BODY=10485760 VERBOSE=1 ./webserv_exhaustion_tests.zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
TARGET_URL=${TARGET_URL:-/}
UPLOAD_URL=${UPLOAD_URL:-/cgi-test/echo}
MAX_BODY=${MAX_BODY:-10485760}      # 10 MB; matches "->{} 10MB" config default
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
print -- "upload:    $UPLOAD_URL"
print -- "max body:  $MAX_BODY"
print -- ""

WEBPID=$(pgrep -nf '/webserv' 2>/dev/null | head -n1 || true)
fd_count() {
  if [[ -n $WEBPID && -d /proc/$WEBPID/fd ]]; then
    ls /proc/$WEBPID/fd 2>/dev/null | wc -l | tr -d ' '
  else
    echo "?"
  fi
}

run_test() {
  local name=$1 fn=$2 out
  out=$($fn 2>&1); local rc=$?
  if (( rc == 0 )); then
    print -r -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— $out${C_OFF}"
    (( PASS++ ))
  else
    print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— $out${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
  fi
}

# -----------------------------------------------------------------------------
# EX1 — single huge header value (16 KiB).
t_ex1_oversized_header() {
  HOST=$HOST PORT=$PORT "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT'])
huge='A'*16384
req=(f'GET / HTTP/1.1\r\nHost: x\r\nX-Huge: {huge}\r\nConnection: close\r\n\r\n').encode()
s=socket.create_connection((host, port), timeout=10); s.settimeout(10)
try:
    s.sendall(req); data=b''
    while True:
        try: c=s.recv(65536)
        except socket.timeout: break
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
    if code in ('400','413','414','431'):
        sys.stdout.write(f'code={code}')
    else:
        sys.stderr.write(f'unexpected {code} (expected 4xx)\n'); sys.exit(1)
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# EX2 — 1000 individual headers.
t_ex2_many_headers() {
  HOST=$HOST PORT=$PORT "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT'])
hdrs='\r\n'.join(f'X-H{i}: v' for i in range(1000))
req=(f'GET / HTTP/1.1\r\nHost: x\r\n{hdrs}\r\nConnection: close\r\n\r\n').encode()
s=socket.create_connection((host, port), timeout=10); s.settimeout(10)
try:
    s.sendall(req); data=b''
    while True:
        try: c=s.recv(65536)
        except socket.timeout: break
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
    if code in ('400','413','414','431'):
        sys.stdout.write(f'code={code}')
    else:
        sys.stderr.write(f'unexpected {code} (expected 4xx)\n'); sys.exit(1)
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# EX3 — request line over 8 KiB (oversized URI).
t_ex3_oversized_uri() {
  HOST=$HOST PORT=$PORT "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT'])
url='/'+'a'*8192
req=(f'GET {url} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n').encode()
s=socket.create_connection((host, port), timeout=10); s.settimeout(10)
try:
    s.sendall(req); data=b''
    while True:
        try: c=s.recv(65536)
        except socket.timeout: break
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
    if code in ('400','414','413','431'):
        sys.stdout.write(f'code={code}')
    else:
        sys.stderr.write(f'unexpected {code} (expected 4xx)\n'); sys.exit(1)
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# EX4 — 10 GiB body bomb. Server must reject before the body arrives;
# we send only the headers and look for an immediate 4xx.
t_ex4_body_bomb() {
  HOST=$HOST PORT=$PORT UP=$UPLOAD_URL "$PYTHON" - <<'PY'
import os, socket, sys, time
host=os.environ['HOST']; port=int(os.environ['PORT']); up=os.environ['UP']
req=(f'POST {up} HTTP/1.1\r\nHost: x\r\nContent-Length: 10737418240\r\n'
     f'Connection: close\r\n\r\n').encode()
s=socket.create_connection((host, port), timeout=10); s.settimeout(10)
t0=time.time()
try:
    s.sendall(req)
    data=b''
    while time.time()-t0 < 5:
        try: c=s.recv(65536)
        except socket.timeout: break
        if not c: break
        data+=c
        if data.find(b'\r\n\r\n') >= 0: break
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
    if code in ('400','413'):
        sys.stdout.write(f'code={code} in {time.time()-t0:.1f}s')
    else:
        sys.stderr.write(f'unexpected {code} (expected 400/413)\n'); sys.exit(1)
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# EX5 — body exactly at cap. Should NOT be rejected as 413.
t_ex5_at_cap() {
  CAP=$MAX_BODY HOST=$HOST PORT=$PORT UP=$UPLOAD_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT'])
cap=int(os.environ['CAP']); up=os.environ['UP']
req=(f'POST {up} HTTP/1.1\r\nHost: x\r\nContent-Length: {cap}\r\n'
     f'Content-Type: application/octet-stream\r\nConnection: close\r\n\r\n').encode()
s=socket.create_connection((host, port), timeout=60); s.settimeout(60)
try:
    s.sendall(req)
    chunk=b'A'*65536; sent=0
    while sent < cap:
        n=min(len(chunk), cap-sent)
        s.sendall(chunk[:n]); sent += n
    data=b''
    while True:
        try: c=s.recv(65536)
        except socket.timeout: break
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
    if code == '413':
        sys.stderr.write(f'at-cap rejected as 413 (expected ≤cap accepted)\n'); sys.exit(1)
    sys.stdout.write(f'code={code}')
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# EX6 — body cap+1. Must be rejected with 413.
t_ex6_over_cap() {
  CAP=$MAX_BODY HOST=$HOST PORT=$PORT UP=$UPLOAD_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT'])
cap=int(os.environ['CAP'])+1; up=os.environ['UP']
req=(f'POST {up} HTTP/1.1\r\nHost: x\r\nContent-Length: {cap}\r\n'
     f'Connection: close\r\n\r\n').encode()
s=socket.create_connection((host, port), timeout=10); s.settimeout(10)
try:
    s.sendall(req)
    data=b''
    while True:
        try: c=s.recv(65536)
        except socket.timeout: break
        if not c: break
        data+=c
        if data.find(b'\r\n\r\n') >= 0: break
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
    if code == '413':
        sys.stdout.write(f'code={code}')
    else:
        sys.stderr.write(f'expected 413, got {code}\n'); sys.exit(1)
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# EX7 — connection flood (500 connections opened then closed). Server must
# stay responsive immediately afterwards.
t_ex7_connection_flood() {
  local fd_before fd_after code
  fd_before=$(fd_count)
  HOST=$HOST PORT=$PORT "$PYTHON" - <<'PY' >/dev/null
import socket, os
host=os.environ['HOST']; port=int(os.environ['PORT'])
socks=[]
for i in range(500):
    try:
        s=socket.create_connection((host, port), timeout=2); s.settimeout(2)
        socks.append(s)
    except Exception:
        break
for s in socks:
    try: s.close()
    except: pass
PY
  sleep 0.5
  code=$(HOST=$HOST PORT=$PORT P=$TARGET_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT']); p=os.environ['P']
try:
    s=socket.create_connection((host, port), timeout=5)
    s.sendall(f'GET {p} HTTP/1.1\r\nHost:x\r\nConnection: close\r\n\r\n'.encode())
    data=b''
    while True:
        c=s.recv(8192)
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    sys.stdout.write(line.split(' ')[1] if line.startswith('HTTP/') else '---')
except Exception as e:
    sys.stdout.write(f'ERR:{e}')
PY
)
  fd_after=$(fd_count)
  if [[ $code != 2* && $code != 3* ]]; then
    print -r -- "fast probe got '$code' after flood"; return 1
  fi
  print -r -- "post-flood probe → $code; fd $fd_before→$fd_after"
}

# -----------------------------------------------------------------------------
# EX8 — 5 pipelined requests in a single TCP segment. All must be answered.
t_ex8_pipelined() {
  HOST=$HOST PORT=$PORT P=$TARGET_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT']); p=os.environ['P']
n=5
parts=[f'GET {p} HTTP/1.1\r\nHost: x\r\n\r\n' for _ in range(n-1)]
parts.append(f'GET {p} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
req=''.join(parts).encode()
s=socket.create_connection((host, port), timeout=10); s.settimeout(10)
try:
    s.sendall(req); data=b''
    while True:
        c=s.recv(65536)
        if not c: break
        data+=c
    s.close()
    cnt=data.count(b'HTTP/1.1 ')
    if cnt < n:
        sys.stderr.write(f'got {cnt}/{n} responses\n'); sys.exit(1)
    sys.stdout.write(f'served {cnt}/{n} pipelined')
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# EX9 — Server still answers a normal GET after all of EX1–EX8 ran.
t_ex9_post_exhaustion_health() {
  HOST=$HOST PORT=$PORT P=$TARGET_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT']); p=os.environ['P']
try:
    s=socket.create_connection((host, port), timeout=5)
    s.sendall(f'GET {p} HTTP/1.1\r\nHost:x\r\nConnection: close\r\n\r\n'.encode())
    data=b''
    while True:
        c=s.recv(8192)
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else '---'
    code=line.split(' ')[1] if line.startswith('HTTP/') else '---'
    if code[:1] not in ('2','3'):
        sys.stderr.write(f'health probe → {code}\n'); sys.exit(1)
    sys.stdout.write(f'health probe → {code}')
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

run_test "EX1 oversized header value (16 KiB) → 4xx"     t_ex1_oversized_header
run_test "EX2 1000 headers → 4xx"                         t_ex2_many_headers
run_test "EX3 oversized URI (8 KiB) → 4xx"                t_ex3_oversized_uri
run_test "EX4 10 GiB body bomb rejected before allocation" t_ex4_body_bomb
run_test "EX5 body at cap accepted"                       t_ex5_at_cap
run_test "EX6 body over cap → 413"                        t_ex6_over_cap
run_test "EX7 500-conn flood; server stays responsive"    t_ex7_connection_flood
run_test "EX8 5 pipelined requests served"                t_ex8_pipelined
run_test "EX9 post-exhaustion health"                     t_ex9_post_exhaustion_health

print -- ""
print -- "──────────────────────────────────────────────────────────────"
print -- "passed: $PASS   failed: $FAIL"
if (( FAIL > 0 )); then
  print -- ""
  print -- "failures:"
  for f in "${FAILED[@]}"; do print -- "  - $f"; done
fi
exit $FAIL
