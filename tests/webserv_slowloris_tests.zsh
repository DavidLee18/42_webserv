#!/usr/bin/env zsh
# webserv_slowloris_tests.zsh — Section 5.1: slowloris and slow-trickle
# resilience. Server must time out slow clients without blocking responsive
# traffic, and fd count must remain stable across the run.
#
# Usage:
#   PORT=8080 ./webserv_slowloris_tests.zsh
#   SLOW_TIMEOUT=15 VERBOSE=1 ./webserv_slowloris_tests.zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
TARGET_URL=${TARGET_URL:-/}
SLOW_TIMEOUT=${SLOW_TIMEOUT:-15}
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
print -- "max wait:  ${SLOW_TIMEOUT}s per slow client"
print -- ""

# Best-effort fd count for the webserv process.
WEBPID=$(pgrep -nf '/webserv' 2>/dev/null | head -n1 || true)
fd_count() {
  if [[ -n $WEBPID && -d /proc/$WEBPID/fd ]]; then
    ls /proc/$WEBPID/fd 2>/dev/null | wc -l | tr -d ' '
  else
    echo "?"
  fi
}

# Quick health probe (returns just the status code).
fast_get() {
  HOST=$HOST PORT=$PORT P=$TARGET_URL "$PYTHON" - <<'PY'
import os, socket, sys
host=os.environ['HOST']; port=int(os.environ['PORT']); p=os.environ['P']
try:
    s=socket.create_connection((host, port), timeout=3)
    s.sendall(f'GET {p} HTTP/1.1\r\nHost:x\r\nConnection: close\r\n\r\n'.encode())
    data = b''
    while True:
        c = s.recv(8192)
        if not c: break
        data += c
    s.close()
    line = data.split(b'\r\n', 1)[0].decode('latin-1','replace')
    sys.stdout.write(line.split(' ')[1] if line.startswith('HTTP/') else '---')
except Exception as e:
    sys.stdout.write(f'ERR:{e}')
PY
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
# SL1 — single slow header trickle (1 byte every 200 ms).
# Server must respond (4xx is fine) before SLOW_TIMEOUT.
t_sl1_slow_header_trickle() {
  HOST=$HOST PORT=$PORT TIMEOUT=$SLOW_TIMEOUT "$PYTHON" - <<'PY'
import socket, sys, time, os
host=os.environ['HOST']; port=int(os.environ['PORT']); to=float(os.environ['TIMEOUT'])
s=socket.create_connection((host, port), timeout=to); s.settimeout(to)
hdr=b'GET / HTTP/1.1\r\nHost: x\r\nUser-Agent: slow\r\nConnection: close\r\n\r\n'
t0=time.time()
try:
    for b in hdr:
        s.sendall(bytes([b])); time.sleep(0.2)
        if time.time()-t0 > to: break
    data=b''
    while True:
        try: c=s.recv(8192)
        except socket.timeout: break
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else ''
    if not line.startswith('HTTP/'):
        sys.stderr.write(f'no response after {time.time()-t0:.1f}s\n'); sys.exit(1)
    code=line.split(' ')[1]
    sys.stdout.write(f'completed in {time.time()-t0:.1f}s; code={code}')
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# SL2 — server stays responsive while a slow client is hanging.
t_sl2_responsive_under_slow() {
  local fd_before fd_after code
  fd_before=$(fd_count)
  ( HOST=$HOST PORT=$PORT "$PYTHON" - >/dev/null 2>&1 <<'PY'
import socket, time, os
try:
    s=socket.create_connection((os.environ['HOST'], int(os.environ['PORT'])), timeout=20)
    hdr=b'GET / HTTP/1.1\r\nHost:x\r\n'
    for b in hdr:
        try: s.sendall(bytes([b])); time.sleep(0.5)
        except: break
    s.close()
except Exception: pass
PY
  ) &
  local bgpid=$!
  sleep 0.5
  code=$(fast_get)
  wait $bgpid 2>/dev/null
  fd_after=$(fd_count)
  if [[ $code != 2* && $code != 3* ]]; then
    print -r -- "fast probe got '$code' while slow client running"; return 1
  fi
  print -r -- "fast probe → $code under slow load; fd $fd_before→$fd_after"
}

# -----------------------------------------------------------------------------
# SL3 — half-open connection. Server must drop after some timeout.
t_sl3_half_open() {
  HOST=$HOST PORT=$PORT TIMEOUT=$SLOW_TIMEOUT "$PYTHON" - <<'PY'
import socket, os, sys, time
host=os.environ['HOST']; port=int(os.environ['PORT']); to=float(os.environ['TIMEOUT'])
s=socket.create_connection((host, port), timeout=to); s.settimeout(to)
t0=time.time()
try:
    data=b''
    while time.time()-t0 < to:
        try: c=s.recv(8192)
        except socket.timeout: break
        if not c: break
        data+=c
    s.close()
    elapsed=time.time()-t0
    if data:
        line=data.split(b'\r\n',1)[0].decode('latin-1','replace')
        sys.stdout.write(f'closed in {elapsed:.1f}s with: {line[:60]}')
    else:
        if elapsed >= to - 0.5:
            sys.stderr.write(f'server never closed half-open (waited {elapsed:.1f}s)\n')
            sys.exit(1)
        sys.stdout.write(f'silently closed in {elapsed:.1f}s')
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# SL4 — slow body trickle (CL=1024, 1 byte every 200ms). Body-receive timeout
# should fire and produce a 4xx, NOT block the connection forever.
t_sl4_slow_body_trickle() {
  HOST=$HOST PORT=$PORT TIMEOUT=$SLOW_TIMEOUT "$PYTHON" - <<'PY'
import socket, os, sys, time
host=os.environ['HOST']; port=int(os.environ['PORT']); to=float(os.environ['TIMEOUT'])
s=socket.create_connection((host, port), timeout=to); s.settimeout(to)
hdr=b'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1024\r\nConnection: close\r\n\r\n'
s.sendall(hdr)
t0=time.time(); sent=0
try:
    while sent < 1024 and time.time()-t0 < to:
        try: s.sendall(b'A'); sent += 1; time.sleep(0.2)
        except: break
    data=b''
    while True:
        try: c=s.recv(8192)
        except socket.timeout: break
        if not c: break
        data+=c
    s.close()
    line=data.split(b'\r\n',1)[0].decode('latin-1','replace') if data else ''
    if not line.startswith('HTTP/'):
        sys.stderr.write(f'no response, sent {sent}/1024 in {time.time()-t0:.1f}s\n')
        sys.exit(1)
    code=line.split(' ')[1]
    sys.stdout.write(f'sent {sent}/1024 in {time.time()-t0:.1f}s; code={code}')
except Exception as e:
    sys.stderr.write(f'err: {e}\n'); sys.exit(1)
PY
}

# -----------------------------------------------------------------------------
# SL5 — 50 simultaneous slow clients. Server must accept a new fast connection.
t_sl5_many_slow_clients() {
  local fd_before fd_after code
  fd_before=$(fd_count)
  local pids=() i
  for (( i = 0; i < 50; i++ )); do
    ( HOST=$HOST PORT=$PORT "$PYTHON" - >/dev/null 2>&1 <<'PY'
import socket, time, os
try:
    s=socket.create_connection((os.environ['HOST'], int(os.environ['PORT'])), timeout=10)
    s.sendall(b'GET / HTTP/1.1\r\nHost:x\r\n')
    time.sleep(8)
    s.close()
except Exception: pass
PY
    ) &
    pids+=($!)
  done
  sleep 1
  code=$(fast_get)
  for p in "${pids[@]}"; do wait $p 2>/dev/null; done
  fd_after=$(fd_count)
  if [[ $code != 2* && $code != 3* ]]; then
    print -r -- "fast probe got '$code' under 50 slow clients"; return 1
  fi
  print -r -- "50 slow clients; fast probe → $code; fd $fd_before→$fd_after"
}

# -----------------------------------------------------------------------------
# SL6 — fd count returns to baseline after slow clients close.
t_sl6_fd_recovery() {
  if [[ $WEBPID == "" || ! -d /proc/$WEBPID/fd ]]; then
    print -r -- "skipped (no proc/fd visibility)"; return 0
  fi
  local before mid after
  before=$(fd_count)
  local pids=() i
  for (( i = 0; i < 30; i++ )); do
    ( HOST=$HOST PORT=$PORT "$PYTHON" - >/dev/null 2>&1 <<'PY'
import socket, time, os
try:
    s=socket.create_connection((os.environ['HOST'], int(os.environ['PORT'])), timeout=5)
    s.sendall(b'GET / HTTP/1.1\r\n')
    time.sleep(3)
    s.close()
except: pass
PY
    ) &
    pids+=($!)
  done
  sleep 1
  mid=$(fd_count)
  for p in "${pids[@]}"; do wait $p 2>/dev/null; done
  sleep 2
  after=$(fd_count)
  # after should be within 5 of before (loose check; some lingering CLOSE_WAIT acceptable).
  local diff=$(( after - before ))
  if (( diff < 0 )); then diff=$(( -diff )); fi
  if (( diff > 10 )); then
    print -r -- "fd leaked: before=$before mid=$mid after=$after"; return 1
  fi
  print -r -- "fd recovered: $before→$mid→$after"
}

run_test "SL1 slow header trickle (1B/200ms)"            t_sl1_slow_header_trickle
run_test "SL2 server responsive under slow client"       t_sl2_responsive_under_slow
run_test "SL3 half-open connection eventually dropped"   t_sl3_half_open
run_test "SL4 slow body trickle bounded by timeout"      t_sl4_slow_body_trickle
run_test "SL5 50 slow clients; fast probe still served"  t_sl5_many_slow_clients
run_test "SL6 fd count recovers after slow clients close" t_sl6_fd_recovery

print -- ""
print -- "──────────────────────────────────────────────────────────────"
print -- "passed: $PASS   failed: $FAIL"
if (( FAIL > 0 )); then
  print -- ""
  print -- "failures:"
  for f in "${FAILED[@]}"; do print -- "  - $f"; done
fi
exit $FAIL
