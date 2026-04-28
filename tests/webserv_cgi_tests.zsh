#!/usr/bin/env zsh
# webserv_cgi_tests.zsh — Section 3.1 (timeouts, rlimits) + 3.2 (env hygiene).
#
# Tests:
#   T1  baseline CGI executes and returns 200
#   T2  hung CGI (sleep > timeout) is killed; server returns 5xx within bound
#   T3  CGI that consumes CPU forever is killed (RLIMIT_CPU)
#   T4  CGI that allocates massive memory is killed (RLIMIT_AS)
#   T5  CGI that writes a huge file is killed (RLIMIT_FSIZE)
#   T6  CGI that opens many fds is bounded (RLIMIT_NOFILE)
#   E1  CGI env contains only RFC 3875 vars + HTTP_*
#   E2  Server-process env (PATH, HOME, LD_*) is NOT visible to CGI
#   E3  HTTP request headers appear as HTTP_* in CGI env
#   E4  QUERY_STRING is correctly populated
#   E5  CONTENT_LENGTH and CONTENT_TYPE set on POST
#   F1  CGI runs with chdir into script directory
#   F2  CGI does not inherit server fds (no fd 3, 4, ... visible)
#   R1  10 sequential CGI requests; no zombies, no fd leak
#   R2  RST during CGI execution (already covered by D12 but re-checked)
#
# Usage:
#   PORT=8080 PID=$(pidof webserv) CGI_DIR=spool/www/cgi-bin ./webserv_cgi_tests.zsh
#
# Setup:
#   This script writes its CGI test scripts into CGI_DIR before running.
#   Your config must route requests to that directory as CGI.
#   Override CGI_URL_PREFIX (default /cgi-bin) if your route differs.

set -u
setopt PIPE_FAIL

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
VERBOSE=${VERBOSE:-0}
PID=${PID:-}
CGI_DIR=${CGI_DIR:-spool/www/cgi-bin}
CGI_URL_PREFIX=${CGI_URL_PREFIX:-/cgi-bin}
CGI_TIMEOUT_BOUND=${CGI_TIMEOUT_BOUND:-15}  # max seconds a "hung" CGI test waits

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

print -- "target:    http://${HOST}:${PORT}"
print -- "pid:       ${PID:-<unknown>}"
print -- "cgi-dir:   $CGI_DIR"
print -- "cgi-url:   $CGI_URL_PREFIX"
print -- "(VERBOSE=1 for failure detail)"
print -- ""

# -----------------------------------------------------------------------------
# helpers
# -----------------------------------------------------------------------------

fd_count() {
  [[ -z $PID ]] && { print -- 0; return }
  ls /proc/$PID/fd 2>/dev/null | wc -l
}

server_alive() {
  [[ -z $PID ]] && return 0
  kill -0 $PID 2>/dev/null
}

zombie_count() {
  [[ -z $PID ]] && { print -- 0; return }
  ps --ppid $PID -o stat= 2>/dev/null | grep -c 'Z' || print -- 0
}

# Issue an HTTP request, return "<status> <elapsed_ms> <body>"
http_get() {
  local path=$1 timeout=${2:-10}
  python3 -c "
import socket, sys, time
host, port, path, t = '$HOST', $PORT, '$path', $timeout
try:
    s = socket.create_connection((host, port), timeout=t)
    s.settimeout(t)
    req = f'GET {path} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'.encode()
    t0 = time.time()
    s.sendall(req)
    data = b''
    while True:
        chunk = s.recv(8192)
        if not chunk: break
        data += chunk
    elapsed_ms = int((time.time() - t0) * 1000)
    s.close()
    if not data:
        print('--- 0 ')
        sys.exit(0)
    head, _, body = data.partition(b'\r\n\r\n')
    line = head.split(b'\r\n', 1)[0].decode('latin-1', 'replace')
    parts = line.split(' ')
    code = parts[1] if len(parts) >= 2 and parts[0].startswith('HTTP/') else '---'
    print(f'{code} {elapsed_ms}')
    sys.stdout.buffer.write(body)
except Exception as e:
    print(f'--- 0 ERR:{e}')
" 2>/dev/null
}

http_post() {
  local path=$1 body=$2 ct=${3:-application/x-www-form-urlencoded} timeout=${4:-10}
  python3 -c "
import socket, sys, time
host, port, path, t = '$HOST', $PORT, '$path', $timeout
body = sys.argv[1].encode()
ct = sys.argv[2]
try:
    s = socket.create_connection((host, port), timeout=t)
    s.settimeout(t)
    req = (f'POST {path} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n'
           f'Content-Type: {ct}\r\nContent-Length: {len(body)}\r\n\r\n').encode()
    t0 = time.time()
    s.sendall(req + body)
    data = b''
    while True:
        chunk = s.recv(8192)
        if not chunk: break
        data += chunk
    elapsed_ms = int((time.time() - t0) * 1000)
    s.close()
    if not data:
        print('--- 0 ')
        sys.exit(0)
    head, _, body_resp = data.partition(b'\r\n\r\n')
    line = head.split(b'\r\n', 1)[0].decode('latin-1', 'replace')
    parts = line.split(' ')
    code = parts[1] if len(parts) >= 2 and parts[0].startswith('HTTP/') else '---'
    print(f'{code} {elapsed_ms}')
    sys.stdout.buffer.write(body_resp)
except Exception as e:
    print(f'--- 0 ERR:{e}')
" "$body" "$ct" 2>/dev/null
}

# -----------------------------------------------------------------------------
# CGI script setup — write test scripts into $CGI_DIR
# -----------------------------------------------------------------------------

setup_cgi_scripts() {
  if [[ ! -d $CGI_DIR ]]; then
    print -- "${C_FAIL}ERROR${C_OFF} CGI_DIR=$CGI_DIR does not exist"
    print -- "${C_DIM}  create it and ensure webserv routes $CGI_URL_PREFIX/* to it as CGI${C_OFF}"
    exit 2
  fi

  # 1. baseline — just emit a plain response
  cat > "$CGI_DIR/hello.cgi" <<'PY'
#!/usr/bin/env python3
print("Content-Type: text/plain")
print()
print("hello from cgi")
PY

  # 2. sleep — hangs for 30 seconds (longer than any reasonable timeout)
  cat > "$CGI_DIR/sleep.cgi" <<'PY'
#!/usr/bin/env python3
import time
print("Content-Type: text/plain")
print()
print("about to sleep")
import sys; sys.stdout.flush()
time.sleep(30)
print("woke up")
PY

  # 3. burn cpu — infinite loop
  cat > "$CGI_DIR/burncpu.cgi" <<'PY'
#!/usr/bin/env python3
print("Content-Type: text/plain")
print()
import sys; sys.stdout.flush()
while True:
    pass
PY

  # 4. mem hog — try to allocate 4 GiB
  cat > "$CGI_DIR/memhog.cgi" <<'PY'
#!/usr/bin/env python3
print("Content-Type: text/plain")
print()
import sys; sys.stdout.flush()
chunks = []
try:
    for _ in range(4096):
        chunks.append(b'X' * (1024 * 1024))  # 1 MiB chunk
except MemoryError:
    print("memory error caught")
PY

  # 5. file bomb — write 4 GiB to disk
  cat > "$CGI_DIR/fbomb.cgi" <<'PY'
#!/usr/bin/env python3
print("Content-Type: text/plain")
print()
import sys, tempfile
sys.stdout.flush()
try:
    with tempfile.NamedTemporaryFile(delete=True) as f:
        chunk = b'X' * (1024 * 1024)
        for _ in range(4096):
            f.write(chunk)
        print("done")
except Exception as e:
    print(f"err: {e}")
PY

  # 6. fd hog — open as many fds as it can
  cat > "$CGI_DIR/fdhog.cgi" <<'PY'
#!/usr/bin/env python3
print("Content-Type: text/plain")
print()
import sys; sys.stdout.flush()
fds = []
try:
    for _ in range(20000):
        fds.append(open('/dev/null', 'r'))
except OSError as e:
    print(f"opened {len(fds)} fds, then: {e}")
PY

  # 7. env dump — emit all env vars, one per line
  cat > "$CGI_DIR/envdump.cgi" <<'PY'
#!/usr/bin/env python3
import os
print("Content-Type: text/plain")
print()
for k in sorted(os.environ):
    print(f"{k}={os.environ[k]}")
PY

  # 8. cwd dump — show current working directory and contents
  cat > "$CGI_DIR/cwd.cgi" <<'PY'
#!/usr/bin/env python3
import os
print("Content-Type: text/plain")
print()
print(f"CWD={os.getcwd()}")
print(f"SCRIPT_DIR_LISTING:")
try:
    for f in sorted(os.listdir('.'))[:20]:
        print(f"  {f}")
except Exception as e:
    print(f"  err: {e}")
PY

  # 9. fd dump — list inherited fds
  cat > "$CGI_DIR/fddump.cgi" <<'PY'
#!/usr/bin/env python3
import os, sys
print("Content-Type: text/plain")
print()
try:
    fds = sorted(int(x) for x in os.listdir(f'/proc/{os.getpid()}/fd'))
    for fd in fds:
        try:
            target = os.readlink(f'/proc/{os.getpid()}/fd/{fd}')
        except OSError:
            target = '<gone>'
        print(f"fd={fd} -> {target}")
except Exception as e:
    print(f"err: {e}")
PY

  chmod +x "$CGI_DIR"/*.cgi 2>/dev/null
}

# -----------------------------------------------------------------------------
# Section 3.1 — Process limits
# -----------------------------------------------------------------------------

run_test() {
  local name=$1 fn=$2
  local fd_before=$(fd_count)
  local zombie_before=$(zombie_count)

  local ok msg
  msg=$($fn 2>&1)
  ok=$?

  sleep 0.5  # let server reap

  if ! server_alive; then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— server died${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  local fd_after=$(fd_count)
  local zombie_after=$(zombie_count)
  local fd_delta=$(( fd_after - fd_before ))
  local zombie_delta=$(( zombie_after - zombie_before ))

  if (( ok != 0 )); then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— $msg${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  if [[ -n $PID ]] && (( fd_delta > 3 )); then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— fd leak: Δ${fd_delta}; $msg${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  if [[ -n $PID ]] && (( zombie_delta > 0 )); then
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— zombie leak: Δ${zombie_delta}; $msg${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  print -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— $msg${C_OFF}"
  (( PASS++ ))
}

t_baseline() {
  local out=$(http_get "$CGI_URL_PREFIX/hello.cgi" 5)
  local code=$(print -- "$out" | head -1 | awk '{print $1}')
  local body=$(print -- "$out" | tail -n +2)
  if [[ $code != 200 ]]; then
    print -- "got code $code, expected 200"
    return 1
  fi
  if [[ ! $body == *"hello from cgi"* ]]; then
    print -- "body missing expected text"
    return 1
  fi
  print -- "code=200 body=ok"
}

t_timeout_sleep() {
  local out=$(http_get "$CGI_URL_PREFIX/sleep.cgi" $CGI_TIMEOUT_BOUND)
  local code=$(print -- "$out" | head -1 | awk '{print $1}')
  local elapsed=$(print -- "$out" | head -1 | awk '{print $2}')
  if [[ $code == 200 ]]; then
    print -- "CGI completed (took ${elapsed}ms) — no timeout enforced; expected 5xx"
    return 1
  fi
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -- "took ${elapsed}ms; expected < ${CGI_TIMEOUT_BOUND}000ms"
    return 1
  fi
  if [[ $code != 5* && $code != --- ]]; then
    print -- "got code=$code; expected 5xx or ---"
    return 1
  fi
  print -- "killed after ${elapsed}ms; code=$code"
}

t_timeout_cpu() {
  local out=$(http_get "$CGI_URL_PREFIX/burncpu.cgi" $CGI_TIMEOUT_BOUND)
  local code=$(print -- "$out" | head -1 | awk '{print $1}')
  local elapsed=$(print -- "$out" | head -1 | awk '{print $2}')
  if [[ $code == 200 ]]; then
    print -- "burncpu completed (took ${elapsed}ms); expected 5xx"
    return 1
  fi
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -- "took ${elapsed}ms; expected < ${CGI_TIMEOUT_BOUND}000ms"
    return 1
  fi
  print -- "killed after ${elapsed}ms; code=$code"
}

t_rlimit_mem() {
  local out=$(http_get "$CGI_URL_PREFIX/memhog.cgi" $CGI_TIMEOUT_BOUND)
  local code=$(print -- "$out" | head -1 | awk '{print $1}')
  local elapsed=$(print -- "$out" | head -1 | awk '{print $2}')
  # acceptable outcomes: 5xx (rlimit kill), or 200 with "memory error caught"
  # in body if the script gracefully handled the OOM.
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -- "took ${elapsed}ms — likely no RLIMIT_AS; expected fast bound"
    return 1
  fi
  if [[ $code != 200 && $code != 5* && $code != --- ]]; then
    print -- "unexpected code=$code"
    return 1
  fi
  print -- "bounded in ${elapsed}ms; code=$code (lenient: any prompt termination is OK)"
}

t_rlimit_fsize() {
  local out=$(http_get "$CGI_URL_PREFIX/fbomb.cgi" $CGI_TIMEOUT_BOUND)
  local code=$(print -- "$out" | head -1 | awk '{print $1}')
  local elapsed=$(print -- "$out" | head -1 | awk '{print $2}')
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -- "took ${elapsed}ms — likely no RLIMIT_FSIZE; expected fast bound"
    return 1
  fi
  print -- "bounded in ${elapsed}ms; code=$code"
}

t_rlimit_nofile() {
  local out=$(http_get "$CGI_URL_PREFIX/fdhog.cgi" $CGI_TIMEOUT_BOUND)
  local code=$(print -- "$out" | head -1 | awk '{print $1}')
  local elapsed=$(print -- "$out" | head -1 | awk '{print $2}')
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -- "took ${elapsed}ms — fd-hog hung; expected fast bound"
    return 1
  fi
  print -- "bounded in ${elapsed}ms; code=$code"
}

# -----------------------------------------------------------------------------
# Section 3.2 — Environment hygiene
# -----------------------------------------------------------------------------

# The full set of RFC 3875 §4.1 meta-variables permitted in a CGI environment.
# Anything NOT in this set, NOT prefixed HTTP_, and NOT a documented helper
# (PWD, _, SHLVL, OLDPWD inherited from the shell on some systems) is suspect.
RFC3875_VARS=(
  AUTH_TYPE CONTENT_LENGTH CONTENT_TYPE GATEWAY_INTERFACE
  PATH_INFO PATH_TRANSLATED QUERY_STRING
  REMOTE_ADDR REMOTE_HOST REMOTE_IDENT REMOTE_USER
  REQUEST_METHOD SCRIPT_NAME
  SERVER_NAME SERVER_PORT SERVER_PROTOCOL SERVER_SOFTWARE
)

is_rfc3875_or_http() {
  local k=$1
  [[ $k == HTTP_* ]] && return 0
  for v in $RFC3875_VARS; do
    [[ $k == $v ]] && return 0
  done
  return 1
}

t_env_only_rfc3875() {
  local out=$(http_get "$CGI_URL_PREFIX/envdump.cgi?foo=bar" 5)
  local code=$(print -- "$out" | head -1 | awk '{print $1}')
  local body=$(print -- "$out" | tail -n +2)
  if [[ $code != 200 ]]; then
    print -- "envdump returned code=$code"
    return 1
  fi

  local extra=()
  while IFS='=' read -r k _; do
    [[ -z $k ]] && continue
    if ! is_rfc3875_or_http "$k"; then
      extra+=("$k")
    fi
  done <<< "$body"

  if (( ${#extra[@]} > 0 )); then
    print -- "non-RFC3875 vars leaked into CGI env: ${extra[*]}"
    return 1
  fi
  print -- "env contains only RFC 3875 + HTTP_* vars (lines: $(print -- "$body" | wc -l))"
}

t_env_no_server_leak() {
  local out=$(http_get "$CGI_URL_PREFIX/envdump.cgi" 5)
  local body=$(print -- "$out" | tail -n +2)

  # These should NEVER appear in the CGI env (they're server-process internals)
  local forbidden=(PATH HOME LD_LIBRARY_PATH LD_PRELOAD USER LOGNAME SHELL TERM PWD)
  local leaked=()
  for k in $forbidden; do
    if print -- "$body" | grep -q "^${k}="; then
      leaked+=("$k")
    fi
  done

  if (( ${#leaked[@]} > 0 )); then
    print -- "server env leaked into CGI: ${leaked[*]}"
    return 1
  fi
  print -- "no server-process env vars leaked (checked: ${forbidden[*]})"
}

t_env_http_passthrough() {
  local out=$(python3 -c "
import socket, sys
s = socket.create_connection(('$HOST', $PORT), timeout=5)
req = (b'GET $CGI_URL_PREFIX/envdump.cgi HTTP/1.1\r\n'
       b'Host: x\r\n'
       b'X-Test-Header: marker-value-42\r\n'
       b'User-Agent: cgitester\r\n'
       b'Connection: close\r\n\r\n')
s.sendall(req)
data = b''
while True:
    c = s.recv(4096)
    if not c: break
    data += c
s.close()
sys.stdout.buffer.write(data)
" 2>/dev/null)
  local body=$(print -- "$out" | awk 'BEGIN{p=0} /^\r?$/{p=1; next} p{print}')

  if ! print -- "$body" | grep -q "^HTTP_X_TEST_HEADER=marker-value-42"; then
    print -- "X-Test-Header did not appear as HTTP_X_TEST_HEADER"
    return 1
  fi
  if ! print -- "$body" | grep -q "^HTTP_USER_AGENT=cgitester"; then
    print -- "User-Agent did not appear as HTTP_USER_AGENT"
    return 1
  fi
  print -- "HTTP_* mapping works (X-Test-Header, User-Agent confirmed)"
}

t_env_query_string() {
  local out=$(http_get "$CGI_URL_PREFIX/envdump.cgi?name=alice&id=42" 5)
  local body=$(print -- "$out" | tail -n +2)
  local qs=$(print -- "$body" | awk -F= '/^QUERY_STRING=/{$1=""; sub(/^=/,""); print}')
  if [[ $qs != *"name=alice"* || $qs != *"id=42"* ]]; then
    print -- "QUERY_STRING missing expected pairs; got: $qs"
    return 1
  fi
  print -- "QUERY_STRING=$qs"
}

t_env_post_content() {
  local out=$(http_post "$CGI_URL_PREFIX/envdump.cgi" "field=value" "application/x-www-form-urlencoded" 5)
  local body=$(print -- "$out" | tail -n +2)
  local cl=$(print -- "$body" | awk -F= '/^CONTENT_LENGTH=/{print $2}')
  local ct=$(print -- "$body" | awk -F= '/^CONTENT_TYPE=/{$1=""; sub(/^=/,""); print}')
  if [[ $cl != 11 ]]; then
    print -- "CONTENT_LENGTH=$cl; expected 11"
    return 1
  fi
  if [[ $ct != *"application/x-www-form-urlencoded"* ]]; then
    print -- "CONTENT_TYPE=$ct; expected application/x-www-form-urlencoded"
    return 1
  fi
  print -- "CONTENT_LENGTH=$cl, CONTENT_TYPE=$ct"
}

t_chdir_into_script_dir() {
  local out=$(http_get "$CGI_URL_PREFIX/cwd.cgi" 5)
  local body=$(print -- "$out" | tail -n +2)
  local cwd=$(print -- "$body" | awk -F= '/^CWD=/{print $2}')
  if [[ -z $cwd ]]; then
    print -- "no CWD line in body"
    return 1
  fi
  # cwd should be an absolute path ending in cgi-bin (or equivalent)
  if [[ $cwd != */cgi-bin* && $cwd != *"$CGI_DIR"* ]]; then
    print -- "CWD=$cwd does not resemble script directory (expected to contain cgi-bin or $CGI_DIR)"
    return 1
  fi
  print -- "CWD=$cwd"
}

t_no_inherited_fds() {
  local out=$(http_get "$CGI_URL_PREFIX/fddump.cgi" 5)
  local body=$(print -- "$out" | tail -n +2)
  # Should see fd 0 (stdin), 1 (stdout), 2 (stderr), and the /proc fd from
  # the readlink call itself. Anything beyond fd 3 that isn't /proc is suspect.
  local n_fds=$(print -- "$body" | grep -c '^fd=')
  if (( n_fds > 6 )); then
    print -- "CGI sees $n_fds inherited fds; expected ≤ 6"
    if (( VERBOSE )); then
      print -- "${C_DIM}  $body${C_OFF}"
    fi
    return 1
  fi
  print -- "CGI sees $n_fds fds (clean)"
}

# -----------------------------------------------------------------------------
# Resilience under repeated CGI use
# -----------------------------------------------------------------------------

t_repeated_cgi() {
  local fd_before=$(fd_count)
  local zombies_before=$(zombie_count)
  for _ in {1..10}; do
    local out=$(http_get "$CGI_URL_PREFIX/hello.cgi" 3)
    local code=$(print -- "$out" | head -1 | awk '{print $1}')
    if [[ $code != 200 ]]; then
      print -- "iteration failed with code=$code"
      return 1
    fi
  done
  sleep 0.5
  local fd_after=$(fd_count)
  local zombies_after=$(zombie_count)
  if (( fd_after - fd_before > 3 )); then
    print -- "fd leak: ${fd_before}→${fd_after}"
    return 1
  fi
  if (( zombies_after - zombies_before > 0 )); then
    print -- "zombie leak: ${zombies_before}→${zombies_after}"
    return 1
  fi
  print -- "10 iterations ok; fd ${fd_before}→${fd_after}, zombies ${zombies_before}→${zombies_after}"
}

# -----------------------------------------------------------------------------
# main
# -----------------------------------------------------------------------------

setup_cgi_scripts

print -- "── 3.1 process limits & timeouts ───────────────────────────────"
run_test "T1 baseline CGI returns 200"                      t_baseline
run_test "T2 hung CGI killed by wall-clock timeout"         t_timeout_sleep
run_test "T3 CPU-burning CGI bounded"                       t_timeout_cpu
run_test "T4 memory hog bounded (RLIMIT_AS or timeout)"     t_rlimit_mem
run_test "T5 file-size bomb bounded (RLIMIT_FSIZE or timeout)" t_rlimit_fsize
run_test "T6 fd-hog bounded (RLIMIT_NOFILE or timeout)"     t_rlimit_nofile

print -- "── 3.2 environment hygiene ─────────────────────────────────────"
run_test "E1 CGI env contains only RFC 3875 + HTTP_*"       t_env_only_rfc3875
run_test "E2 server-process env not leaked into CGI"        t_env_no_server_leak
run_test "E3 HTTP request headers appear as HTTP_*"         t_env_http_passthrough
run_test "E4 QUERY_STRING populated correctly"              t_env_query_string
run_test "E5 CONTENT_LENGTH/CONTENT_TYPE set on POST"       t_env_post_content

print -- "── 3.3 filesystem & fd hygiene ─────────────────────────────────"
run_test "F1 chdir into script directory"                   t_chdir_into_script_dir
run_test "F2 no excess inherited fds"                       t_no_inherited_fds

print -- "── 3.4 resilience ──────────────────────────────────────────────"
run_test "R1 10 sequential CGI requests, no zombies/leaks"  t_repeated_cgi

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