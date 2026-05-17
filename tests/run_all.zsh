#!/usr/bin/env zsh
# run_all.zsh — drive every webserv test suite in sequence and aggregate.
# Assumes webserv is already running on $HOST:$PORT.
#
# Usage:
#   tests/run_all.zsh
#   HOST=127.0.0.1 PORT=8080 PYTHON=$(pyenv which python3) tests/run_all.zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
PYTHON=${PYTHON:-$(command -v python3)}

# Sensible defaults for suite-specific env vars.
export HOST PORT PYTHON
export UPLOAD_URL=${UPLOAD_URL:-/cgi-test/echo}
export ECHO_CGI_URL=${ECHO_CGI_URL:-/cgi-test/echo}
export UPLOAD_PATH=${UPLOAD_PATH:-/storage/}
export CGI_EXTRA_ENV_KEYS=${CGI_EXTRA_ENV_KEYS:-LC_CTYPE}
export SKIP_CONFIRM=1

if [[ -t 1 ]]; then
  C_HEAD=$'\e[1;36m'; C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'
  C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_HEAD=''; C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

SCRIPT_DIR=${0:A:h}

typeset -a SUITES
SUITES=(
  "webserv_parsing_tests.zsh"
  "webserv_headers_tests.zsh"
  "webserv_cgi_framing_tests.zsh"
  "webserv_cgi_tests.zsh"
  "webserv_chunked_tests.zsh"
  "webserv_disconnect_tests.zsh"
  "webserv_conditional_tests.zsh"
  "webserv_slowloris_tests.zsh"
  "webserv_exhaustion_tests.zsh"
  "webserv_eval_smoke_tests.zsh"
  "webserv_multi_cgi_tests.zsh"
)

# Sanity probe: is the server up?
if ! "$PYTHON" - <<PY
import socket, sys
try:
    s = socket.create_connection(("$HOST", $PORT), timeout=2); s.close()
except Exception as e:
    sys.stderr.write(f'cannot reach $HOST:$PORT — start webserv first ({e})\n')
    sys.exit(1)
PY
then
  exit 1
fi

print -- ""
print -- "${C_HEAD}═══ webserv test runner ═══${C_OFF}"
print -- "${C_DIM}target: http://${HOST}:${PORT}${C_OFF}"
print -- "${C_DIM}suites: ${#SUITES[@]}${C_OFF}"

TOTAL_PASS=0
TOTAL_FAIL=0
typeset -a FAILED_SUITES MISSING_SUITES
FAILED_SUITES=()
MISSING_SUITES=()

for suite in "${SUITES[@]}"; do
  script="$SCRIPT_DIR/$suite"
  if [[ ! -e $script ]]; then
    MISSING_SUITES+=("$suite")
    print -- ""
    print -- "${C_DIM}── skip $suite (not found) ──${C_OFF}"
    continue
  fi
  if [[ ! -x $script ]]; then
    chmod +x "$script" 2>/dev/null
  fi
  print -- ""
  print -- "${C_HEAD}━━━ $suite ━━━${C_OFF}"
  logfile=$(mktemp)
  "$script" 2>&1 | tee "$logfile"
  rc=${pipestatus[1]}
  # Strip ANSI then parse the suite's "passed: X   failed: Y" line.
  summary=$(sed -E $'s/\x1b\\[[0-9;]*m//g' "$logfile" | awk '/^passed: /{print; exit}')
  p=$(print -r -- "$summary" | awk '{print $2}')
  f=$(print -r -- "$summary" | awk '{print $4}')
  [[ -z $p ]] && p=0
  [[ -z $f ]] && f=0
  TOTAL_PASS=$(( TOTAL_PASS + p ))
  TOTAL_FAIL=$(( TOTAL_FAIL + f ))
  if (( rc != 0 || f > 0 )); then
    FAILED_SUITES+=("$suite ($f failed)")
  fi
  rm -f "$logfile"
done

print -- ""
print -- "${C_HEAD}═══ aggregate ═══${C_OFF}"
print -- "  total passed: ${C_PASS}$TOTAL_PASS${C_OFF}"
print -- "  total failed: ${C_FAIL}$TOTAL_FAIL${C_OFF}"
if (( ${#MISSING_SUITES[@]} > 0 )); then
  print -- ""
  print -- "  ${C_DIM}missing suites:${C_OFF}"
  for s in "${MISSING_SUITES[@]}"; do print -- "    - $s"; done
fi
if (( ${#FAILED_SUITES[@]} > 0 )); then
  print -- ""
  print -- "  ${C_FAIL}failing suites:${C_OFF}"
  for s in "${FAILED_SUITES[@]}"; do print -- "    - $s"; done
  exit 1
fi
exit 0
