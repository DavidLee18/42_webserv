#!/usr/bin/env zsh
# webserv_cgi_framing_tests.zsh — B4 CGI-output → HTTP/1.1 framing parser tests.
#
# Builds a tiny C++ binary that links against your framing helper and feeds
# it 12 literal inputs covering the cases that matter:
#   F1  well-formed with Status:        → 200 OK + headers + body
#   F2  Status: 404 Not Found           → status line synthesised
#   F3  Status: 302 + Location          → redirect framing
#   F4  CRLF boundary (\r\n\r\n)        → standard form
#   F5  LF-only boundary  (\n\n)        → tolerated form
#   F6  no Content-Length, has body     → server must compute it
#   F7  CL provided by CGI              → server must trust it
#   F8  empty body, headers only        → CL: 0
#   F9  Set-Cookie pass-through         → forwarded verbatim
#   F10 multiple custom headers         → all forwarded
#   F11 no boundary at all              → must error (502 territory)
#   F12 body containing \r\n\r\n        → first occurrence wins, not last
#
# Usage:
#   ./webserv_cgi_framing_tests.zsh                       # builds + runs
#   FRAME_CALL='Cgi::frame(...)' ./webserv_cgi_framing_tests.zsh
#   KEEP=1 ./webserv_cgi_framing_tests.zsh                # keep the binary
#
# Prerequisites: the framing helper must be implemented and exposed via a
# header reachable from the project root; adjust INCLUDES if not.

emulate -L zsh
set -u

PROJECT_ROOT=${PROJECT_ROOT:-$(pwd)}
BUILD_DIR=${BUILD_DIR:-/tmp/webserv_framing_test}
FRAME_HEADER=${FRAME_HEADER:-src/server/Response.hpp}
FRAME_CALL=${FRAME_CALL:-Response::from_cgi_outbuff}
KEEP=${KEEP:-0}

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

mkdir -p "$BUILD_DIR"
RUNNER="$BUILD_DIR/framing_runner"
SRC="$BUILD_DIR/framing_runner.cpp"

# -----------------------------------------------------------------------------
# C++ runner: reads input from argv[1] (file path), prints framed output to
# stdout on OK, or "ERR: <message>" to stderr on failure.
# -----------------------------------------------------------------------------
cat > "$SRC" <<EOF
#include "$FRAME_HEADER"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: framing_runner <input_file>" << std::endl;
    return 2;
  }

  std::ifstream f(argv[1], std::ios::binary);
  if (!f) {
    std::cerr << "ERR: cannot open input " << argv[1] << std::endl;
    return 2;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  const std::string input = ss.str();

  Result<Response> r = $FRAME_CALL(input, std::map<std::string, std::string>());
  if (!r.has_value()) {
    std::cerr << "ERR: " << r.error() << std::endl;
    return 1;
  }
  std::ostringstream out;
  out << r.value();
  std::cout << out.str();
  return 0;
}
EOF

# -----------------------------------------------------------------------------
# Build. Adjust INCLUDES / SOURCES to your project layout.
# -----------------------------------------------------------------------------
INCLUDES=(-I"$PROJECT_ROOT" -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/src/server")
# Pull in only what the helper transitively needs. Add files here if linking
# fails — typically you'll need the file that defines frame_cgi_output and
# whatever Result<>/Errors symbols it pulls in.
SOURCES=(
  "$PROJECT_ROOT/src/cgi_1_1/CgiAuthType.cpp"
  "$PROJECT_ROOT/src/cgi_1_1/CgiDelegate.cpp"
  "$PROJECT_ROOT/src/cgi_1_1/CgiInput.cpp"
  "$PROJECT_ROOT/src/cgi_1_1/CgiMetaVar.cpp"
  "$PROJECT_ROOT/src/cgi_1_1/ContentType.cpp"
  "$PROJECT_ROOT/src/cgi_1_1/EtcMetaVar.cpp"
  "$PROJECT_ROOT/src/cgi_1_1/ServerName.cpp"
  "$PROJECT_ROOT/src/config/ConfigError.cpp"
  "$PROJECT_ROOT/src/config/PathPattern.cpp"
  "$PROJECT_ROOT/src/config/RouteRule_CGI.cpp"
  "$PROJECT_ROOT/src/config/ServerConfig.cpp"
  "$PROJECT_ROOT/src/server/Response.cpp"
  "$PROJECT_ROOT/src/server/DefaultError.cpp"
  "$PROJECT_ROOT/src/server/Client.cpp"
  "$PROJECT_ROOT/src/server/Server.cpp"
  "$PROJECT_ROOT/src/server/Session.cpp"
  "$PROJECT_ROOT/src/EPoll_KQueue.cpp"
  "$PROJECT_ROOT/src/framing_test_dummy.cpp"
  "$PROJECT_ROOT/src/Errors.cpp"
  "$PROJECT_ROOT/src/FileDescriptor.cpp"
  "$PROJECT_ROOT/src/utils.cpp"
)

print -- "Building runner…"
if ! c++ -std=c++98 -Wall -Wextra -Werror -g -O0 \
      "${INCLUDES[@]}" "$SRC" "${SOURCES[@]}" -o "$RUNNER" 2>&1; then
  print -- "${C_FAIL}BUILD FAILED${C_OFF}"
  print -- "Adjust FRAME_HEADER, FRAME_CALL, INCLUDES, or SOURCES at the top"
  print -- "of $0 to match your project."
  exit 2
fi
print -- "Built $RUNNER"
print -- ""

# -----------------------------------------------------------------------------
# Test machinery
# -----------------------------------------------------------------------------
PASS=0
FAIL=0
typeset -a FAILED
FAILED=()

# Run a single case.
#   $1 name
#   $2 input (raw CGI output)
#   $3 expected_kind: ok | err
#   $4 ok-mode: regex applied with grep -E -z (whole-output match), or empty
#   $5 ok-mode: regex applied with grep -v -E (must NOT match), or empty
run_case() {
  local name=$1 input=$2 kind=$3 must_match=${4:-} must_not_match=${5:-}

  local infile="$BUILD_DIR/input.bin"
  local outfile="$BUILD_DIR/out.bin"
  local errfile="$BUILD_DIR/err.bin"

  print -rn -- "$input" > "$infile"

  "$RUNNER" "$infile" >"$outfile" 2>"$errfile"
  local rc=$?

  local ok=1
  local detail=""

  case $kind in
    ok)
      if (( rc != 0 )); then
        ok=0
        detail="expected ok, got rc=$rc, stderr=$(cat "$errfile")"
      elif [[ -n $must_match ]]; then
        if ! grep -Eqz "$must_match" "$outfile"; then
          ok=0
          detail="output did not match expected pattern: $must_match"
        fi
      fi
      if (( ok )) && [[ -n $must_not_match ]]; then
        if grep -Eqz "$must_not_match" "$outfile"; then
          ok=0
          detail="output matched forbidden pattern: $must_not_match"
        fi
      fi
      ;;
    err)
      if (( rc == 0 )); then
        ok=0
        detail="expected err, got ok with output: $(head -c 200 "$outfile")"
      fi
      ;;
    *)
      ok=0
      detail="bad test kind: $kind"
      ;;
  esac

  if (( ok )); then
    print -- "${C_PASS}PASS${C_OFF} $name"
    (( PASS++ ))
  else
    print -- "${C_FAIL}FAIL${C_OFF} $name ${C_DIM}— $detail${C_OFF}"
    FAILED+=("$name")
    (( FAIL++ ))
  fi
}

# -----------------------------------------------------------------------------
# 12 cases
# -----------------------------------------------------------------------------

# F1: well-formed with explicit Status:
run_case "F1 explicit Status: 200 OK" \
  $'Status: 200 OK\r\nContent-Type: text/plain\r\n\r\nhello' \
  ok \
  $'^HTTP/1\\.1 200 OK\r\n' \
  '(^|[\r\n])Status:'

# F2: Status: 404 Not Found → status line synthesised, Status: stripped
run_case "F2 Status: 404 → status line synthesised" \
  $'Status: 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>Not Found</h1>' \
  ok \
  $'^HTTP/1\\.1 404 Not Found\r\n' \
  '(^|[\r\n])Status:'

# F3: redirect via Status: 302 + Location header
run_case "F3 Status: 302 + Location forwarded" \
  $'Status: 302 Found\r\nLocation: /elsewhere\r\nContent-Type: text/html\r\n\r\n' \
  ok \
  $'Location: /elsewhere\r\n' \
  ''

# F4: standard CRLF boundary
run_case "F4 CRLF boundary" \
  $'Content-Type: text/plain\r\n\r\nbody' \
  ok \
  $'^HTTP/1\\.1 200 OK\r\n' \
  ''

# F5: bare LF boundary should be tolerated
run_case "F5 LF-only boundary tolerated" \
  $'Content-Type: text/plain\n\nbody' \
  ok \
  $'^HTTP/1\\.1 200 OK' \
  ''

# F6: no Content-Length from CGI → server computes it (body = "hello", 5 bytes)
run_case "F6 missing CL → server computes" \
  $'Content-Type: text/plain\r\n\r\nhello' \
  ok \
  $'Content-Length: 5\r\n' \
  ''

# F7: CGI provides Content-Length → server trusts and forwards
run_case "F7 CGI-provided CL trusted" \
  $'Content-Type: text/plain\r\nContent-Length: 5\r\n\r\nhello' \
  ok \
  $'Content-Length: 5\r\n' \
  ''

# F8: empty body, headers only → CL: 0
run_case "F8 empty body → CL: 0" \
  $'Content-Type: text/plain\r\n\r\n' \
  ok \
  $'Content-Length: 0\r\n' \
  ''

# F9: Set-Cookie passed through
run_case "F9 Set-Cookie forwarded" \
  $'Content-Type: text/html\r\nSet-Cookie: sid=abc; Path=/\r\n\r\n<p>hi</p>' \
  ok \
  $'Set-Cookie: sid=abc; Path=/\r\n' \
  ''

# F10: multiple custom headers all forwarded
run_case "F10 multiple custom headers forwarded" \
  $'Content-Type: application/json\r\nX-One: 1\r\nX-Two: 2\r\n\r\n{}' \
  ok \
  $'X-One: 1\r\n.*X-Two: 2\r\n' \
  ''

# F11: no header/body boundary at all → must error
run_case "F11 no boundary → error" \
  $'Content-Type: text/plain (no blank line)' \
  err

# F12: body containing \r\n\r\n → split on FIRST occurrence, not last
# Headers: just Content-Type. Body: "first\r\n\r\nsecond" (15 bytes).
run_case "F12 body containing \\r\\n\\r\\n: first split wins" \
  $'Content-Type: text/plain\r\n\r\nfirst\r\n\r\nsecond' \
  ok \
  $'Content-Length: 15\r\n' \
  ''

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

if (( ! KEEP )); then
  rm -rf "$BUILD_DIR"
fi

exit $FAIL
