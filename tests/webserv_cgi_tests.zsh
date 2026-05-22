#!/usr/bin/env zsh
# webserv_cgi_tests.zsh — Section 3 (CGI sandboxing) for config-driven routing.
#
# Webserv routes CGIs from default.wbsrv entries like:
#   GET /cgi-test/hello    $hello.cgi(KEY=VAL)
#       ...5
#       OTHER_KEY=other-val
#
# This harness:
#   1. writes its test CGIs into $CGI_DIR
#   2. prints the route block to add to your default.wbsrv
#   3. runs the tests once you confirm the server is reloaded
#
# Usage:
#   PORT=8080 PID=$(pidof webserv) CGI_DIR=www-files/cgi-bin \
#     ./webserv_cgi_tests.zsh
#
# Override any test URL via env (CGI_URL_HELLO, CGI_URL_SLEEP, ...).
# CGI_EXTRA_ENV_KEYS lists vars your routes inject (allowed in env hygiene
# tests beyond RFC 3875 + HTTP_*). E.g. CGI_EXTRA_ENV_KEYS="DIST_DIR AUTH_INFO".

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
VERBOSE=${VERBOSE:-0}
PID=${PID:-}
CGI_DIR=${CGI_DIR:-www-files/cgi-bin}
CGI_TIMEOUT_BOUND=${CGI_TIMEOUT_BOUND:-10}
PYTHON=${PYTHON:-$(which python3)}

# URL → CGI mapping (override via env if your routes differ)
CGI_URL_HELLO=${CGI_URL_HELLO:-/cgi-test/hello}
CGI_URL_SLEEP=${CGI_URL_SLEEP:-/cgi-test/sleep}
CGI_URL_BURNCPU=${CGI_URL_BURNCPU:-/cgi-test/burncpu}
CGI_URL_MEMHOG=${CGI_URL_MEMHOG:-/cgi-test/memhog}
CGI_URL_FBOMB=${CGI_URL_FBOMB:-/cgi-test/fbomb}
CGI_URL_FDHOG=${CGI_URL_FDHOG:-/cgi-test/fdhog}
CGI_URL_ENVDUMP=${CGI_URL_ENVDUMP:-/cgi-test/envdump}
CGI_URL_CWD=${CGI_URL_CWD:-/cgi-test/cwd}
CGI_URL_FDDUMP=${CGI_URL_FDDUMP:-/cgi-test/fddump}
CGI_URL_ENVDUMP_POST=${CGI_URL_ENVDUMP_POST:-/cgi-test/envdump-post}

# Vars your route(s) intentionally inject into the CGI env. Whitespace-separated.
CGI_EXTRA_ENV_KEYS=${CGI_EXTRA_ENV_KEYS:-}

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

PASS=0
FAIL=0
typeset -a FAILED
FAILED=()

if [[ -z $PID ]]; then
  PID=$(pgrep -x webserv 2>/dev/null | head -n1)
fi

print -r -- "target:    http://${HOST}:${PORT}"
print -r -- "pid:       ${PID:-<unknown>}"
print -r -- "cgi-dir:   $CGI_DIR"
print -r -- ""

# -----------------------------------------------------------------------------
# helpers
# -----------------------------------------------------------------------------

fd_count() {
  if [[ -z $PID ]]; then print -r -- 0; return; fi
  ls /proc/$PID/fd 2>/dev/null | wc -l | tr -d ' '
}

server_alive() {
  [[ -z $PID ]] && return 0
  kill -0 "$PID" 2>/dev/null
}

zombie_count() {
  if [[ -z $PID ]]; then print -r -- 0; return; fi
  local n
  n=$(ps --ppid "$PID" -o stat= 2>/dev/null | grep -c '^Z')
  [[ -z $n ]] && n=0
  print -r -- "$n"
}

# Issue an HTTP request via raw TCP. Echoes one header line ("<code> <ms>")
# then the response body bytes.
http_request() {
  local method=$1 path=$2 timeout=${3:-10} body=${4:-} ct=${5:-}
  HOST=$HOST PORT=$PORT TIMEOUT=$timeout REQ_METHOD=$method REQ_PATH=$path \
  REQ_BODY=$body REQ_CT=$ct \
    "$PYTHON" - <<'PY'
import os, socket, sys, time
host = os.environ['HOST']
port = int(os.environ['PORT'])
t    = float(os.environ['TIMEOUT'])
m    = os.environ['REQ_METHOD']
path = os.environ['REQ_PATH']
body = os.environ['REQ_BODY'].encode()
ct   = os.environ['REQ_CT']
try:
    s = socket.create_connection((host, port), timeout=t)
    s.settimeout(t)
    headers = [f'{m} {path} HTTP/1.1', 'Host: x', 'Connection: close']
    if body:
        if ct:
            headers.append(f'Content-Type: {ct}')
        headers.append(f'Content-Length: {len(body)}')
    req = ('\r\n'.join(headers) + '\r\n\r\n').encode() + body
    t0 = time.time()
    s.sendall(req)
    data = b''
    while True:
        chunk = s.recv(8192)
        if not chunk:
            break
        data += chunk
    elapsed_ms = int((time.time() - t0) * 1000)
    s.close()
    if not data:
        sys.stdout.write(f'--- {elapsed_ms}\n')
        sys.exit(0)
    head, _, body_resp = data.partition(b'\r\n\r\n')
    line = head.split(b'\r\n', 1)[0].decode('latin-1', 'replace')
    parts = line.split(' ')
    code = parts[1] if len(parts) >= 2 and parts[0].startswith('HTTP/') else '---'
    sys.stdout.write(f'{code} {elapsed_ms}\n')
    sys.stdout.flush()
    sys.stdout.buffer.write(body_resp)
except Exception as e:
    sys.stdout.write(f'--- 0 ERR:{e}\n')
PY
}

# Issue request with extra header lines (used for HTTP_* mapping test)
http_request_with_headers() {
  local method=$1 path=$2 timeout=$3
  shift 3
  local extra_headers=("$@")
  HOST=$HOST PORT=$PORT TIMEOUT=$timeout REQ_METHOD=$method REQ_PATH=$path \
  EXTRA_HDRS="${(j:|:)extra_headers}" \
    "$PYTHON" - <<'PY'
import os, socket, sys, time
host = os.environ['HOST']
port = int(os.environ['PORT'])
t    = float(os.environ['TIMEOUT'])
m    = os.environ['REQ_METHOD']
path = os.environ['REQ_PATH']
extra = [h for h in os.environ['EXTRA_HDRS'].split('|') if h]
try:
    s = socket.create_connection((host, port), timeout=t)
    s.settimeout(t)
    headers = [f'{m} {path} HTTP/1.1', 'Host: x', 'Connection: close', *extra]
    req = ('\r\n'.join(headers) + '\r\n\r\n').encode()
    t0 = time.time()
    s.sendall(req)
    data = b''
    while True:
        chunk = s.recv(8192)
        if not chunk:
            break
        data += chunk
    elapsed_ms = int((time.time() - t0) * 1000)
    s.close()
    if not data:
        sys.stdout.write(f'--- {elapsed_ms}\n')
        sys.exit(0)
    head, _, body_resp = data.partition(b'\r\n\r\n')
    line = head.split(b'\r\n', 1)[0].decode('latin-1', 'replace')
    parts = line.split(' ')
    code = parts[1] if len(parts) >= 2 and parts[0].startswith('HTTP/') else '---'
    sys.stdout.write(f'{code} {elapsed_ms}\n')
    sys.stdout.flush()
    sys.stdout.buffer.write(body_resp)
except Exception as e:
    sys.stdout.write(f'--- 0 ERR:{e}\n')
PY
}

status_of()  { local l1=$(printf '%s\n' "$1" | head -n1); printf '%s' "${l1%% *}"; }
elapsed_of() { local l1=$(printf '%s\n' "$1" | head -n1); local r=${l1#* }; printf '%s' "${r%% *}"; }
body_of()    { printf '%s' "$1" | tail -n +2; }


print_route_block() {
  print -r -- "${C_DIM}── required default.wbsrv route block ──────────────────────${C_OFF}"
  cat <<EOF
GET ${CGI_URL_HELLO}     \$hello.cgi()
GET ${CGI_URL_SLEEP}     \$sleep.cgi()
        ...5
GET ${CGI_URL_BURNCPU}   \$burncpu.cgi()
        ...5
GET ${CGI_URL_MEMHOG}    \$memhog.cgi()
        ...10
GET ${CGI_URL_FBOMB}     \$fbomb.cgi()
        ...10
GET ${CGI_URL_FDHOG}     \$fdhog.cgi()
        ...10
GET ${CGI_URL_ENVDUMP}   \$envdump.cgi()
GET ${CGI_URL_CWD}       \$cwd.cgi()
GET ${CGI_URL_FDDUMP}    \$fddump.cgi()
POST ${CGI_URL_ENVDUMP_POST}   \$envdump.cgi()
EOF
  print -r -- "${C_DIM}─────────────────────────────────────────────────────────────${C_OFF}"
  print -r -- ""
}

# -----------------------------------------------------------------------------
# test runner
# -----------------------------------------------------------------------------

run_test() {
  local name=$1 fn=$2
  local fd_before zombie_before
  fd_before=$(fd_count)
  zombie_before=$(zombie_count)

  local ok msg
  msg=$($fn 2>&1)
  ok=$?

  sleep 0.5

  if ! server_alive; then
    print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— server died${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  local fd_after zombie_after
  fd_after=$(fd_count)
  zombie_after=$(zombie_count)
  local fd_delta=$(( fd_after - fd_before ))
  local zombie_delta=$(( zombie_after - zombie_before ))

  if (( ok != 0 )); then
    print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— $msg${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  if [[ -n $PID ]] && (( fd_delta > 3 )); then
    print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— fd leak Δ${fd_delta}; $msg${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  if [[ -n $PID ]] && (( zombie_delta > 0 )); then
    print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— zombie leak Δ${zombie_delta}; $msg${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
    return
  fi

  print -r -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— $msg${C_OFF}"
  (( PASS++ ))
}

# -----------------------------------------------------------------------------
# 3.1 process limits & timeouts
# -----------------------------------------------------------------------------

t_baseline() {
  local out code body
  out=$(http_request GET "$CGI_URL_HELLO" 5)
  code=$(status_of "$out")
  body=$(body_of "$out")
  if [[ $code != 200 ]]; then
    print -r -- "code=$code expected 200"; return 1
  fi
  if [[ $body != *"hello from cgi"* ]]; then
    print -r -- "body missing 'hello from cgi'"; return 1
  fi
  print -r -- "code=200 body=ok"
}

t_timeout_sleep() {
  local out code elapsed
  out=$(http_request GET "$CGI_URL_SLEEP" "$CGI_TIMEOUT_BOUND")
  code=$(status_of "$out")
  elapsed=$(elapsed_of "$out")
  if [[ $code == 200 ]]; then
    print -r -- "CGI completed in ${elapsed}ms — no timeout enforced"; return 1
  fi
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -r -- "took ${elapsed}ms — exceeded bound ${CGI_TIMEOUT_BOUND}s"; return 1
  fi
  print -r -- "killed in ${elapsed}ms; code=$code"
}

t_timeout_cpu() {
  local out code elapsed
  out=$(http_request GET "$CGI_URL_BURNCPU" "$CGI_TIMEOUT_BOUND")
  code=$(status_of "$out")
  elapsed=$(elapsed_of "$out")
  if [[ $code == 200 ]]; then
    print -r -- "burncpu completed in ${elapsed}ms — not killed"; return 1
  fi
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -r -- "took ${elapsed}ms — not bounded"; return 1
  fi
  print -r -- "killed in ${elapsed}ms; code=$code"
}

t_rlimit_mem() {
  local out code elapsed
  out=$(http_request GET "$CGI_URL_MEMHOG" "$CGI_TIMEOUT_BOUND")
  code=$(status_of "$out")
  elapsed=$(elapsed_of "$out")
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -r -- "took ${elapsed}ms — likely no RLIMIT_AS"; return 1
  fi
  print -r -- "bounded in ${elapsed}ms; code=$code"
}

t_rlimit_fsize() {
  local out code elapsed
  out=$(http_request GET "$CGI_URL_FBOMB" "$CGI_TIMEOUT_BOUND")
  code=$(status_of "$out")
  elapsed=$(elapsed_of "$out")
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -r -- "took ${elapsed}ms — likely no RLIMIT_FSIZE"; return 1
  fi
  print -r -- "bounded in ${elapsed}ms; code=$code"
}

t_rlimit_nofile() {
  local out code elapsed
  out=$(http_request GET "$CGI_URL_FDHOG" "$CGI_TIMEOUT_BOUND")
  code=$(status_of "$out")
  elapsed=$(elapsed_of "$out")
  if (( elapsed > CGI_TIMEOUT_BOUND * 1000 )); then
    print -r -- "took ${elapsed}ms — fd-hog hung"; return 1
  fi
  print -r -- "bounded in ${elapsed}ms; code=$code"
}

# -----------------------------------------------------------------------------
# 3.2 environment hygiene
# -----------------------------------------------------------------------------

RFC3875_VARS=(
  AUTH_TYPE CONTENT_LENGTH CONTENT_TYPE GATEWAY_INTERFACE
  PATH_INFO PATH_TRANSLATED QUERY_STRING
  REMOTE_ADDR REMOTE_HOST REMOTE_IDENT REMOTE_USER
  REQUEST_METHOD SCRIPT_NAME
  SERVER_NAME SERVER_PORT SERVER_PROTOCOL SERVER_SOFTWARE
  REDIRECT_STATUS
)

is_permitted_env_var() {
  local k=$1 v
  [[ $k == HTTP_* ]] && return 0
  for v in "${RFC3875_VARS[@]}"; do
    [[ $k == "$v" ]] && return 0
  done
  for v in ${=CGI_EXTRA_ENV_KEYS}; do
    [[ $k == "$v" ]] && return 0
  done
  return 1
}

t_env_only_permitted() {
  local out body
  out=$(http_request GET "${CGI_URL_ENVDUMP}?foo=bar" 5)
  if [[ $(status_of "$out") != 200 ]]; then
    print -r -- "envdump returned $(status_of "$out")"; return 1
  fi
  body=$(body_of "$out")

  local extra=() k
  while IFS='=' read -r k _; do
    [[ -z $k ]] && continue
    if ! is_permitted_env_var "$k"; then
      extra+=("$k")
    fi
  done <<< "$body"

  if (( ${#extra[@]} > 0 )); then
    print -r -- "non-RFC3875 vars in CGI env: ${extra[*]}"
    if (( VERBOSE )); then
      print -r -- "${C_DIM}  permitted extras (CGI_EXTRA_ENV_KEYS): ${CGI_EXTRA_ENV_KEYS:-<none>}${C_OFF}"
    fi
    return 1
  fi
  print -r -- "env clean ($(printf '%s\n' "$body" | grep -c '^[A-Z]') vars; permitted extras: ${CGI_EXTRA_ENV_KEYS:-<none>})"
}

t_env_no_server_leak() {
  local out body
  out=$(http_request GET "$CGI_URL_ENVDUMP" 5)
  body=$(body_of "$out")

  local forbidden=(PATH HOME LD_LIBRARY_PATH LD_PRELOAD USER LOGNAME SHELL TERM PWD OLDPWD SHLVL)
  local leaked=() k
  for k in "${forbidden[@]}"; do
    if printf '%s\n' "$body" | grep -q "^${k}="; then
      leaked+=("$k")
    fi
  done

  if (( ${#leaked[@]} > 0 )); then
    print -r -- "server env leaked into CGI: ${leaked[*]}"; return 1
  fi
  print -r -- "no server env leaked (checked ${#forbidden[@]} vars)"
}

t_env_http_passthrough() {
  local out body
  out=$(http_request_with_headers GET "$CGI_URL_ENVDUMP" 5 \
        "X-Test-Header: marker-value-42" "User-Agent: cgitester")
  body=$(body_of "$out")

  if ! printf '%s\n' "$body" | grep -q "^HTTP_X_TEST_HEADER=marker-value-42"; then
    print -r -- "X-Test-Header not mapped to HTTP_X_TEST_HEADER"; return 1
  fi
  if ! printf '%s\n' "$body" | grep -q "^HTTP_USER_AGENT=cgitester"; then
    print -r -- "User-Agent not mapped to HTTP_USER_AGENT"; return 1
  fi
  print -r -- "HTTP_* mapping verified"
}

t_env_query_string() {
  local out body qs
  out=$(http_request GET "${CGI_URL_ENVDUMP}?name=alice&id=42" 5)
  body=$(body_of "$out")
  qs=$(printf '%s\n' "$body" | awk '/^QUERY_STRING=/{sub(/^QUERY_STRING=/,""); print; exit}')
  if [[ $qs != *"name=alice"* || $qs != *"id=42"* ]]; then
    print -r -- "QUERY_STRING=$qs missing expected pairs"; return 1
  fi
  print -r -- "QUERY_STRING=$qs"
}

t_env_post_content() {
  local out body cl ct
  out=$(http_request POST "$CGI_URL_ENVDUMP_POST" 5 "field=value" "application/x-www-form-urlencoded")
  if [[ $(status_of "$out") != 200 ]]; then
    print -r -- "POST returned $(status_of "$out")"; return 1
  fi
  body=$(body_of "$out")
  cl=$(printf '%s\n' "$body" | awk -F= '/^CONTENT_LENGTH=/{print $2; exit}')
  ct=$(printf '%s\n' "$body" | awk -F= '/^CONTENT_TYPE=/{$1=""; sub(/^=/,""); print; exit}')
  if [[ $cl != 11 ]]; then
    print -r -- "CONTENT_LENGTH=$cl expected 11"; return 1
  fi
  if [[ $ct != *"application/x-www-form-urlencoded"* ]]; then
    print -r -- "CONTENT_TYPE=$ct unexpected"; return 1
  fi
  print -r -- "CONTENT_LENGTH=$cl, CONTENT_TYPE=$ct"
}

# -----------------------------------------------------------------------------
# 3.3 filesystem & fd hygiene
# -----------------------------------------------------------------------------

t_chdir_into_script_dir() {
  local out body cwd
  out=$(http_request GET "$CGI_URL_CWD" 5)
  body=$(body_of "$out")
  cwd=$(printf '%s\n' "$body" | awk -F= '/^CWD=/{print $2; exit}')
  if [[ -z $cwd ]]; then
    print -r -- "no CWD in body"; return 1
  fi
  # Should resolve to the directory containing cwd.cgi
  local cgi_abs
  cgi_abs=$(cd "$CGI_DIR" 2>/dev/null && pwd)
  if [[ -n $cgi_abs && $cwd != "$cgi_abs"* ]]; then
    if [[ $cwd != *"$CGI_DIR"* && $cwd != *cgi-bin* ]]; then
      print -r -- "CWD=$cwd does not match CGI_DIR ($cgi_abs)"; return 1
    fi
  fi
  print -r -- "CWD=$cwd"
}

t_no_inherited_fds() {
  local out body n_fds
  out=$(http_request GET "$CGI_URL_FDDUMP" 5)
  body=$(body_of "$out")
  n_fds=$(printf '%s\n' "$body" | grep -c '^fd=')
  # 0=stdin, 1=stdout, 2=stderr, plus a /proc fd from readlink loop = ~4
  if (( n_fds > 6 )); then
    print -r -- "CGI sees $n_fds fds (expected ≤6)"
    if (( VERBOSE )); then
      printf '%s\n' "$body" | sed 's/^/    /' >&2
    fi
    return 1
  fi
  print -r -- "$n_fds fds visible (clean)"
}

# -----------------------------------------------------------------------------
# 3.4 resilience
# -----------------------------------------------------------------------------

t_repeated_cgi() {
  local fd_before zombies_before
  fd_before=$(fd_count)
  zombies_before=$(zombie_count)
  local i out code
  for i in {1..10}; do
    out=$(http_request GET "$CGI_URL_HELLO" 3)
    code=$(status_of "$out")
    if [[ $code != 200 ]]; then
      print -r -- "iter $i: code=$code"; return 1
    fi
  done
  sleep 0.5
  local fd_after zombies_after
  fd_after=$(fd_count)
  zombies_after=$(zombie_count)
  if (( fd_after - fd_before > 3 )); then
    print -r -- "fd leak ${fd_before}→${fd_after}"; return 1
  fi
  if (( zombies_after - zombies_before > 0 )); then
    print -r -- "zombie leak ${zombies_before}→${zombies_after}"; return 1
  fi
  print -r -- "10 iters; fd ${fd_before}→${fd_after}, zombies ${zombies_before}→${zombies_after}"
}

# -----------------------------------------------------------------------------
# main
# -----------------------------------------------------------------------------

print_route_block

if [[ ${SKIP_CONFIRM:-0} != 1 ]]; then
  print -r -n -- "Have you added the route block above to default.wbsrv and reloaded webserv? [y/N] "
  read -r ans
  if [[ $ans != [yY]* ]]; then
    print -r -- "aborting; set SKIP_CONFIRM=1 to bypass this prompt"
    exit 0
  fi
  print -r -- ""
fi

print -r -- "── 3.1 process limits & timeouts ──────────────────────────────"
run_test "T1 baseline CGI returns 200"                          t_baseline
run_test "T2 hung CGI killed by wall-clock timeout"             t_timeout_sleep
run_test "T3 CPU-burning CGI bounded"                           t_timeout_cpu
run_test "T4 memory hog bounded (RLIMIT_AS or timeout)"         t_rlimit_mem
run_test "T5 file-size bomb bounded (RLIMIT_FSIZE or timeout)"  t_rlimit_fsize
run_test "T6 fd-hog bounded (RLIMIT_NOFILE or timeout)"         t_rlimit_nofile

print -r -- "── 3.2 environment hygiene ────────────────────────────────────"
run_test "E1 CGI env contains only RFC 3875 + HTTP_* + extras"  t_env_only_permitted
run_test "E2 server-process env not leaked into CGI"            t_env_no_server_leak
run_test "E3 HTTP request headers appear as HTTP_*"             t_env_http_passthrough
run_test "E4 QUERY_STRING populated correctly"                  t_env_query_string
run_test "E5 CONTENT_LENGTH/CONTENT_TYPE set on POST"           t_env_post_content

print -r -- "── 3.3 filesystem & fd hygiene ────────────────────────────────"
run_test "F1 chdir into script directory"                       t_chdir_into_script_dir
run_test "F2 no excess inherited fds"                           t_no_inherited_fds

print -r -- "── 3.4 resilience ─────────────────────────────────────────────"
run_test "R1 10 sequential CGI requests; no zombies/leaks"      t_repeated_cgi

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
