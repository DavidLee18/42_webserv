#!/usr/bin/env zsh
# webserv_multi_cgi_tests.zsh — bonus: multi-interpreter CGI dispatch.
#
# Validates that:
#   M1  .py script  routed to python3 interpreter, returns 200
#   M2  .php script routed to php-cgi  interpreter, returns 200
#   M3  Each interpreter sees its own runtime fingerprint (proves dispatch)
#   M4  POST body reaches each interpreter via stdin
#   M5  QUERY_STRING reaches each interpreter
#   M6  Both interpreters coexist (interleaved requests; no cross-contamination)
#   M7  CONTENT_TYPE / CONTENT_LENGTH set on POST for both
#
# Requires Sir's default.wbsrv to declare two CGI routes pointing at the
# deployed scripts (see "required route block" below).
#
# Usage:
#   ./webserv_multi_cgi_tests.zsh
#   HOST=127.0.0.1 PORT=8080 VERBOSE=1 ./webserv_multi_cgi_tests.zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
CGI_DIR=${CGI_DIR:-./www-files/cgi-bin}
PY_URL=${PY_URL:-/multi-cgi/echo.py}
PHP_URL=${PHP_URL:-/multi-cgi/echo.php}
VERBOSE=${VERBOSE:-0}

PY_BIN=${PY_BIN:-/usr/bin/python3}
PHP_BIN=${PHP_BIN:-/usr/bin/php-cgi}

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_SKIP=$'\e[33m'
  C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_SKIP=''; C_DIM=''; C_OFF=''
fi

PASS=0; FAIL=0; SKIP=0
typeset -a FAILED; FAILED=()

print -- "target:     http://${HOST}:${PORT}"
print -- "cgi-dir:    $CGI_DIR"
print -- "python:     $PY_BIN"
print -- "php-cgi:    $PHP_BIN"
print -- "(VERBOSE=1 to dump responses on failure)"
print -- ""

# -----------------------------------------------------------------------------
# Interpreter availability
# -----------------------------------------------------------------------------

PY_AVAIL=0; PHP_AVAIL=0
[[ -x $PY_BIN  ]] && PY_AVAIL=1
[[ -x $PHP_BIN ]] && PHP_AVAIL=1

if (( PY_AVAIL == 0 && PHP_AVAIL == 0 )); then
  print -- "${C_SKIP}neither python3 nor php-cgi found; suite skipped${C_OFF}"
  exit 0
fi


# -----------------------------------------------------------------------------
# Route block reminder
# -----------------------------------------------------------------------------

print -- "── required default.wbsrv routes ────────────────────────────"
(( PY_AVAIL ))  && print -- "    GET  ${PY_URL}  \$${CGI_DIR#./}/echo.py"
(( PY_AVAIL ))  && print -- "    POST ${PY_URL}  \$${CGI_DIR#./}/echo.py"
(( PHP_AVAIL )) && print -- "    GET  ${PHP_URL} \$${CGI_DIR#./}/echo.php"
(( PHP_AVAIL )) && print -- "    POST ${PHP_URL} \$${CGI_DIR#./}/echo.php"
print -- "─────────────────────────────────────────────────────────────"
print -- ""

# -----------------------------------------------------------------------------
# helpers
# -----------------------------------------------------------------------------

run_test() {
  local name=$1 fn=$2 out rc
  out=$($fn 2>&1); rc=$?
  case $rc in
    0) print -r -- "${C_PASS}PASS${C_OFF} $name ${C_DIM}— $out${C_OFF}"; (( PASS++ ));;
    2) print -r -- "${C_SKIP}SKIP${C_OFF} $name ${C_DIM}— $out${C_OFF}"; (( SKIP++ ));;
    *) print -r -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— $out${C_OFF}"
       FAILED+=("$name"); (( FAIL++ ));;
  esac
}

http_get() {
  # $1=path, $2=timeout-sec
  curl -sS --max-time "${2:-5}" -i "http://${HOST}:${PORT}$1" 2>/dev/null
}

http_post() {
  # $1=path, $2=body, $3=content-type, $4=timeout-sec
  curl -sS --max-time "${4:-5}" -i -X POST \
       -H "Content-Type: $3" \
       --data-binary "$2" \
       "http://${HOST}:${PORT}$1" 2>/dev/null
}

status_of() {
  print -r -- "$1" | awk 'NR==1{print $2; exit}'
}

body_of() {
  print -r -- "$1" | awk 'BEGIN{b=0} /^\r?$/{if(!b){b=1; next}} b{print}'
}

# -----------------------------------------------------------------------------
# tests
# -----------------------------------------------------------------------------

t_m1_python_get() {
  (( PY_AVAIL )) || { print -r -- "python unavailable"; return 2; }
  local out code body
  out=$(http_get "${PY_URL}?probe=m1" 5)
  code=$(status_of "$out")
  body=$(body_of "$out")
  if [[ $code != 200 ]]; then
    print -r -- "expected 200, got $code"
    (( VERBOSE )) && print -r -- "$out" >&2
    return 1
  fi
  if [[ $body != *"INTERPRETER=python"* ]]; then
    print -r -- "body missing INTERPRETER=python marker"
    return 1
  fi
  if [[ $body != *"PYTHON_VERSION="* ]]; then
    print -r -- "body missing PYTHON_VERSION"
    return 1
  fi
  print -r -- "200, python signature confirmed"
}

t_m2_php_get() {
  (( PHP_AVAIL )) || { print -r -- "php-cgi unavailable"; return 2; }
  local out code body
  out=$(http_get "${PHP_URL}?probe=m2" 5)
  code=$(status_of "$out")
  body=$(body_of "$out")
  if [[ $code != 200 ]]; then
    print -r -- "expected 200, got $code"
    (( VERBOSE )) && print -r -- "$out" >&2
    return 1
  fi
  if [[ $body != *"INTERPRETER=php"* ]]; then
    print -r -- "body missing INTERPRETER=php marker"
    return 1
  fi
  if [[ $body != *"PHP_VERSION="* ]]; then
    print -r -- "body missing PHP_VERSION"
    return 1
  fi
  print -r -- "200, php signature confirmed"
}

t_m3_dispatch_isolation() {
  (( PY_AVAIL && PHP_AVAIL )) || { print -r -- "need both interpreters"; return 2; }
  local py_out php_out
  py_out=$(http_get  "$PY_URL"  5)
  php_out=$(http_get "$PHP_URL" 5)
  # The python response must not contain PHP_VERSION;
  # the php response must not contain PYTHON_VERSION.
  if [[ $(body_of "$py_out") == *"PHP_VERSION"* ]]; then
    print -r -- ".py response leaked PHP_VERSION (interpreter cross-talk)"
    return 1
  fi
  if [[ $(body_of "$php_out") == *"PYTHON_VERSION"* ]]; then
    print -r -- ".php response leaked PYTHON_VERSION (interpreter cross-talk)"
    return 1
  fi
  print -r -- "dispatch isolated; .py↛php, .php↛python"
}

t_m4_python_post_body() {
  (( PY_AVAIL )) || { print -r -- "python unavailable"; return 2; }
  local out code body marker
  marker="multi-cgi-body-$$-$(date +%s)"
  out=$(http_post "$PY_URL" "$marker" "text/plain" 5)
  code=$(status_of "$out")
  body=$(body_of "$out")
  if [[ $code != 200 ]]; then
    print -r -- "expected 200, got $code"
    return 1
  fi
  if [[ $body != *"BODY=${marker}"* ]]; then
    print -r -- "body marker absent from python response"
    (( VERBOSE )) && print -r -- "$body" >&2
    return 1
  fi
  print -r -- "python received POST body ($(printf %s "$marker" | wc -c) bytes)"
}

t_m5_php_post_body() {
  (( PHP_AVAIL )) || { print -r -- "php-cgi unavailable"; return 2; }
  local out code body marker
  marker="multi-cgi-body-$$-$(date +%s)"
  out=$(http_post "$PHP_URL" "$marker" "text/plain" 5)
  code=$(status_of "$out")
  body=$(body_of "$out")
  if [[ $code != 200 ]]; then
    print -r -- "expected 200, got $code"
    return 1
  fi
  if [[ $body != *"BODY=${marker}"* ]]; then
    print -r -- "body marker absent from php response"
    (( VERBOSE )) && print -r -- "$body" >&2
    return 1
  fi
  print -r -- "php received POST body ($(printf %s "$marker" | wc -c) bytes)"
}

t_m6_query_string_both() {
  (( PY_AVAIL )) || (( PHP_AVAIL )) || { print -r -- "no interpreter"; return 2; }
  local py_pass=1 php_pass=1 out body
  if (( PY_AVAIL )); then
    out=$(http_get "${PY_URL}?lang=py&id=42" 5)
    body=$(body_of "$out")
    [[ $body == *"QUERY_STRING=lang=py&id=42"* ]] || py_pass=0
  fi
  if (( PHP_AVAIL )); then
    out=$(http_get "${PHP_URL}?lang=php&id=99" 5)
    body=$(body_of "$out")
    [[ $body == *"QUERY_STRING=lang=php&id=99"* ]] || php_pass=0
  fi
  if (( PY_AVAIL && !py_pass )); then
    print -r -- "python QUERY_STRING mismatch"; return 1
  fi
  if (( PHP_AVAIL && !php_pass )); then
    print -r -- "php QUERY_STRING mismatch"; return 1
  fi
  print -r -- "QUERY_STRING reached interpreter(s) intact"
}

t_m7_interleaved() {
  (( PY_AVAIL && PHP_AVAIL )) || { print -r -- "need both interpreters"; return 2; }
  local i out body
  for i in 1 2 3; do
    out=$(http_get "$PY_URL" 5)
    body=$(body_of "$out")
    if [[ $body != *"INTERPRETER=python"* ]]; then
      print -r -- "iter $i python turn returned non-python"; return 1
    fi
    out=$(http_get "$PHP_URL" 5)
    body=$(body_of "$out")
    if [[ $body != *"INTERPRETER=php"* ]]; then
      print -r -- "iter $i php turn returned non-php"; return 1
    fi
  done
  print -r -- "3 interleaved py↔php rounds; dispatch stable"
}

t_m8_content_meta() {
  (( PY_AVAIL )) || (( PHP_AVAIL )) || { print -r -- "no interpreter"; return 2; }
  local marker body
  marker="abc=123"
  if (( PY_AVAIL )); then
    body=$(body_of "$(http_post "$PY_URL" "$marker" "application/x-www-form-urlencoded" 5)")
    [[ $body == *"CONTENT_LENGTH=7"* ]] || { print -r -- "python CL wrong"; return 1; }
    [[ $body == *"CONTENT_TYPE=application/x-www-form-urlencoded"* ]] || \
      { print -r -- "python CT wrong"; return 1; }
  fi
  if (( PHP_AVAIL )); then
    body=$(body_of "$(http_post "$PHP_URL" "$marker" "application/x-www-form-urlencoded" 5)")
    [[ $body == *"CONTENT_LENGTH=7"* ]] || { print -r -- "php CL wrong"; return 1; }
    [[ $body == *"CONTENT_TYPE=application/x-www-form-urlencoded"* ]] || \
      { print -r -- "php CT wrong"; return 1; }
  fi
  print -r -- "CONTENT_LENGTH/TYPE forwarded to interpreter(s)"
}

# -----------------------------------------------------------------------------
# Runner
# -----------------------------------------------------------------------------

print -- "── multi-CGI dispatch ─────────────────────────────────────────"
run_test "M1 .py  GET  → python interpreter"           t_m1_python_get
run_test "M2 .php GET  → php-cgi interpreter"          t_m2_php_get
run_test "M3 dispatch isolation (no cross-talk)"       t_m3_dispatch_isolation
run_test "M4 .py  POST body via stdin"                 t_m4_python_post_body
run_test "M5 .php POST body via stdin"                 t_m5_php_post_body
run_test "M6 QUERY_STRING reaches both interpreters"   t_m6_query_string_both
run_test "M7 interleaved py↔php (3 rounds)"            t_m7_interleaved
run_test "M8 CONTENT_LENGTH/TYPE forwarded on POST"    t_m8_content_meta

print -- ""
print -- "──────────────────────────────────────────────────────────────"
print -- "passed: $PASS   failed: $FAIL   skipped: $SKIP"
if (( FAIL > 0 )); then
  print -- ""; print -- "failures:"
  for f in "${FAILED[@]}"; do print -- "  - $f"; done
fi
exit $FAIL
