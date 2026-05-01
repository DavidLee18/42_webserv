#!/usr/bin/env zsh
# webserv_disconnect_tests.zsh — Section 5.3, mid-request disconnect tests.
# v2: per-iteration try/except so transient connection failures don't abort.

set -u
setopt PIPE_FAIL

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
VERBOSE=${VERBOSE:-0}
PID=${PID:-}
ITERATIONS=${ITERATIONS:-50}
FD_TOLERANCE=${FD_TOLERANCE:-5}
RSS_TOLERANCE_KB=${RSS_TOLERANCE_KB:-2048}
INTER_TEST_SLEEP=${INTER_TEST_SLEEP:-0.5}
MIN_SUCCESS_RATIO=${MIN_SUCCESS_RATIO:-50}

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

PASS=0; FAIL=0; FAILED=()

if [[ -z $PID ]]; then
  PID=$(pgrep -x webserv 2>/dev/null | head -1)
fi
if [[ -z $PID ]]; then
  print -- "${C_FAIL}WARN${C_OFF} no webserv pid found; fd/RSS leak checks will be skipped"
fi

print -- "target:  http://${HOST}:${PORT}"
print -- "pid:     ${PID:-<unknown>}"
print -- "iters:   $ITERATIONS per leak-probe test"
print -- "(VERBOSE=1 for failure detail)"
print -- ""

fd_count() {
  [[ -z $PID ]] && { print -- 0; return }
  ls /proc/$PID/fd 2>/dev/null | wc -l
}

rss_kb() {
  [[ -z $PID ]] && { print -- 0; return }
  awk '/^VmRSS:/{print $2}' /proc/$PID/status 2>/dev/null
}

server_alive() {
  [[ -z $PID ]] && return 0
  kill -0 $PID 2>/dev/null
}

baseline_get() {
  python3 -c "
import socket, sys
try:
    s = socket.create_connection(('$HOST', $PORT), timeout=3)
    s.sendall(b'GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    data = b''
    while True:
        chunk = s.recv(4096)
        if not chunk: break
        data += chunk
        if len(data) > 4096: break
    s.close()
    line = data.split(b'\r\n', 1)[0].decode('latin-1', 'replace')
    parts = line.split(' ')
    print(parts[1] if len(parts) >= 2 and parts[0].startswith('HTTP/') else '---')
except Exception:
    print('---')
" 2>/dev/null
}

# Common boilerplate prepended to every routine: argv parsing, counters,
# per-iteration try/except. Each test only supplies the body of the loop.
make_routine() {
  local body=$1
  cat <<PY
import socket, struct, sys, time, random
host = sys.argv[1]
port = int(sys.argv[2])
n = int(sys.argv[3])
succ = 0
failures = 0
for _i in range(n):
    try:
$body
        succ += 1
    except Exception:
        failures += 1
    time.sleep(0.005)
print(f"OK {succ} {failures}")
PY
}

run_leak_test() {
  local name=$1 pyscript=$2 iters=${3:-$ITERATIONS}

  local fd_before rss_before fd_after rss_after baseline_after result
  fd_before=$(fd_count)
  rss_before=$(rss_kb)

  result=$(python3 -c "$pyscript" "$HOST" "$PORT" "$iters" 2>/dev/null)
  local rc=$?

  local succ fail_count
  if [[ $result =~ '^OK ([0-9]+) ([0-9]+)$' ]]; then
    succ=${match[1]}
    fail_count=${match[2]}
  else
    succ=0
    fail_count=$iters
  fi

  local min_required=$(( iters * MIN_SUCCESS_RATIO / 100 ))

  if (( succ < min_required )); then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— only $succ/$iters iterations succeeded (need ≥$min_required); rc=$rc${C_OFF}"
    if (( VERBOSE )); then
      print -- "${C_DIM}  raw output: $result${C_OFF}"
    fi
    FAILED+=("$name")
    (( FAIL++ ))
    sleep $INTER_TEST_SLEEP
    return
  fi

  sleep 0.3

  if ! server_alive; then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— server process died${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  baseline_after=$(baseline_get)
  if [[ ! $baseline_after =~ ^[2-4][0-9][0-9]$ ]]; then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— baseline GET after disconnect storm returned '$baseline_after' (succ=$succ/$iters)${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    sleep $INTER_TEST_SLEEP
    return
  fi

  fd_after=$(fd_count)
  rss_after=$(rss_kb)
  local fd_delta=$(( fd_after - fd_before ))
  local rss_delta=$(( rss_after - rss_before ))

  local detail="succ=$succ/$iters, fd ${fd_before}→${fd_after} (Δ${fd_delta}), rss ${rss_before}→${rss_after} KiB (Δ${rss_delta})"

  if [[ -n $PID ]] && (( fd_delta > FD_TOLERANCE )); then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— fd leak: $detail${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    sleep $INTER_TEST_SLEEP
    return
  fi

  if [[ -n $PID ]] && (( rss_delta > RSS_TOLERANCE_KB )); then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— RSS growth: $detail${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    sleep $INTER_TEST_SLEEP
    return
  fi

  print -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— $detail, baseline=$baseline_after${C_OFF}"
  (( PASS++ ))
  sleep $INTER_TEST_SLEEP
}

# -----------------------------------------------------------------------------
# Section 5.3 — mid-request disconnect
# -----------------------------------------------------------------------------

print -- "── 5.3.1 disconnect during headers ────────────────────────────"

run_leak_test "D1 RST after partial request-line" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"GET /partial")
        s.close()
')"

run_leak_test "D2 RST after request-line, before any header" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"GET / HTTP/1.1\r\n")
        s.close()
')"

run_leak_test "D3 RST mid header line" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nX-Partial: incompl")
        s.close()
')"

run_leak_test "D4 graceful FIN before header terminator" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nUser-Agent: test")
        s.shutdown(socket.SHUT_WR)
        try:
            while s.recv(4096):
                pass
        except Exception:
            pass
        s.close()
')"

print -- "── 5.3.2 disconnect during body ───────────────────────────────"

run_leak_test "D5 RST after headers, body promised but not sent" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"POST /upload HTTP/1.1\r\nHost: x\r\nContent-Length: 1024\r\n\r\n")
        s.close()
')"

run_leak_test "D6 RST mid-body (sent half of declared CL)" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"POST /upload HTTP/1.1\r\nHost: x\r\nContent-Length: 1024\r\n\r\n")
        s.sendall(b"A" * 512)
        s.close()
')"

run_leak_test "D7 RST exactly at body boundary" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"POST /upload HTTP/1.1\r\nHost: x\r\nContent-Length: 16\r\n\r\nABCDEFGHIJKLMNOP")
        s.close()
')"

run_leak_test "D8 graceful FIN mid-body" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.sendall(b"POST /upload HTTP/1.1\r\nHost: x\r\nContent-Length: 4096\r\n\r\n")
        s.sendall(b"X" * 1024)
        s.shutdown(socket.SHUT_WR)
        try:
            while s.recv(4096):
                pass
        except Exception:
            pass
        s.close()
')"

print -- "── 5.3.3 disconnect during response ───────────────────────────"

run_leak_test "D9 RST immediately after headers, never read response" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
        s.close()
')"

run_leak_test "D10 read 1 byte then RST mid-response" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
        try:
            s.recv(1)
        except Exception:
            pass
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.close()
')"

run_leak_test "D11 stalled reader (never recv, hold socket open briefly)" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
        time.sleep(0.05)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.close()
')" 20

print -- "── 5.3.4 disconnect during CGI ────────────────────────────────"

CGI_PATH=${CGI_PATH:-/cgi/sleep.cgi}
run_leak_test "D12 RST while CGI is presumed running" "$(make_routine "
        s = socket.create_connection((host, port), timeout=2)
        req = b'GET ${CGI_PATH} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'
        s.sendall(req)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
        s.close()
")" 10

print -- "── 5.3.5 mixed adversarial burst ──────────────────────────────"

run_leak_test "D13 100 clients, random disconnect phase each" "$(make_routine '
        s = socket.create_connection((host, port), timeout=2)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        phase = random.randint(0, 6)
        if phase >= 1: s.sendall(b"GET / HTTP/1.1")
        if phase >= 2: s.sendall(b"\r\n")
        if phase >= 3: s.sendall(b"Host: x\r\n")
        if phase >= 4: s.sendall(b"User-Agent: chaos\r\n")
        if phase >= 5: s.sendall(b"\r\n")
        if phase >= 6:
            try: s.recv(1)
            except Exception: pass
        s.close()
')" 100

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
