#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
TEST_DIR="$ROOT_DIR/tests/integration"
TMP_DIR="$TEST_DIR/tmp"
mkdir -p "$TMP_DIR"
UWSGI_SCRIPT="$TMP_DIR/login.py"
PORT_WAIT_RETRIES=60

WEB_PID=""
UWSGI_PID=""

cleanup() {
  if [ -n "$WEB_PID" ]; then
    kill "$WEB_PID" >/dev/null 2>&1 || true
    wait "$WEB_PID" >/dev/null 2>&1 || true
  fi
  if [ -n "$UWSGI_PID" ]; then
    kill "$UWSGI_PID" >/dev/null 2>&1 || true
    wait "$UWSGI_PID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

assert_contains() {
  local file="$1"
  local needle="$2"
  if ! grep -Fq "$needle" "$file"; then
    echo "Assertion failed: expected '$needle' in $file"
    echo "---- $file ----"
    cat "$file"
    echo "---------------"
    exit 1
  fi
}

wait_for_port() {
  local port="$1"
  local retries="$PORT_WAIT_RETRIES"
  while [ "$retries" -gt 0 ]; do
    if python3 - "$port" <<'PY'
import socket
import sys
port = int(sys.argv[1])
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(0.2)
try:
    s.connect(("127.0.0.1", port))
except Exception:
    sys.exit(1)
finally:
    s.close()
sys.exit(0)
PY
    then
      return 0
    fi
    retries=$((retries - 1))
    sleep 0.1
  done
  return 1
}

echo "[integration] Building binaries"
make -C "$ROOT_DIR" all >/dev/null

echo "[integration] Preparing uWSGI fixture script"
cat >"$UWSGI_SCRIPT" <<'EOF'
#!/usr/bin/env python3
import os
import sys

def out(msg):
    sys.stdout.write(msg)

out("Status: 200 OK\r\n")
out("Content-Type: text/plain\r\n")
out("\r\n")
out("uwsgi-ok\n")
out("REQUEST_METHOD=%s\n" % os.environ.get("REQUEST_METHOD", ""))
out("SCRIPT_NAME=%s\n" % os.environ.get("SCRIPT_NAME", ""))
EOF
chmod +x "$UWSGI_SCRIPT"

echo "[integration] Validating CGI integration via parser dump"
CGI_PARSE_OUT="$TMP_DIR/cgi_parse.out"
if [ ! -f "$ROOT_DIR/default.wbsrv" ]; then
  echo "Missing config: $ROOT_DIR/default.wbsrv"
  exit 1
fi
"$ROOT_DIR/webserv" "$ROOT_DIR/default.wbsrv" >"$CGI_PARSE_OUT" 2>&1
assert_contains "$CGI_PARSE_OUT" "Executable: upload_file.cgi"
assert_contains "$CGI_PARSE_OUT" "key: add_csp_sha256.cgi"
assert_contains "$CGI_PARSE_OUT" "Env key: UPLOAD_DEST, Env value: /uploaded/"
assert_contains "$CGI_PARSE_OUT" "Timeout: 5"

echo "[integration] Starting uWSGI server fixture"
"$ROOT_DIR/uwsgi_server" "$UWSGI_SCRIPT" 9000 >"$TMP_DIR/uwsgi_server.log" 2>&1 &
UWSGI_PID="$!"
if ! wait_for_port 9000; then
  echo "uWSGI server did not start on 9000"
  exit 1
fi

echo "[integration] Running uWSGI client end-to-end request"
cat >"$TMP_DIR/uwsgi_client_test.cpp" <<'EOF'
#include "uwsgi_client.h"
#include <iostream>
#include <map>

int main() {
  std::map<std::string, std::string> vars;
  vars["REQUEST_METHOD"] = "POST";
  vars["SCRIPT_NAME"] = "/login";
  vars["CONTENT_LENGTH"] = "0";
  vars["SERVER_PROTOCOL"] = "HTTP/1.1";
  vars["SERVER_NAME"] = "localhost";
  vars["SERVER_PORT"] = "8040";
  vars["REMOTE_ADDR"] = "127.0.0.1";
  UwsgiClient client("127.0.0.1", 9000);
  Result<std::string> result = client.send(vars, "");
  if (!result.has_value()) {
    std::cerr << result.error() << std::endl;
    return 1;
  }
  std::cout << result.value();
  return 0;
}
EOF

"${CXX:-c++}" -Wall -Werror -Wextra -Wconversion -std=c++98 \
  -I"$ROOT_DIR/src" \
  "$TMP_DIR/uwsgi_client_test.cpp" \
  "$ROOT_DIR/src/uwsgi_client.cpp" \
  -o "$TMP_DIR/uwsgi_client_test"

UWSGI_RESP_OUT="$TMP_DIR/uwsgi_response.out"
"$TMP_DIR/uwsgi_client_test" >"$UWSGI_RESP_OUT" 2>&1
assert_contains "$UWSGI_RESP_OUT" "Status: 200 OK"
assert_contains "$UWSGI_RESP_OUT" "uwsgi-ok"
assert_contains "$UWSGI_RESP_OUT" "REQUEST_METHOD=POST"
assert_contains "$UWSGI_RESP_OUT" "SCRIPT_NAME=/login"

echo "[integration] CGI + uWSGI integration suite passed"
