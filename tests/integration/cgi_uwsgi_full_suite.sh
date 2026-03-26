#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_DIR="$(mktemp -d)"
UWSGI_APP="$TMP_DIR/login.py"
PORT_UWSGI=19000
MAX_RETRIES=100

UWSGI_PID=

cleanup() {
  if [[ -n "${UWSGI_PID}" ]] && kill -0 "${UWSGI_PID}" 2>/dev/null; then
    kill "${UWSGI_PID}" || true
    wait "${UWSGI_PID}" 2>/dev/null || true
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

cat >"$UWSGI_APP" <<'PY'
import json
import sys

def main():
    body = sys.stdin.read()
    env = {
        "REQUEST_METHOD": "",
        "PATH_INFO": "",
        "QUERY_STRING": "",
        "CONTENT_TYPE": "",
        "CONTENT_LENGTH": "",
        "SERVER_NAME": "",
        "SERVER_PORT": "",
        "REMOTE_ADDR": "",
        "HTTP_HOST": "",
    }
    for k in env:
        env[k] = __import__("os").environ.get(k, "")
    payload = {"env": env, "body": body}
    raw = json.dumps(payload, sort_keys=True)
    out = [
        "HTTP/1.1 200 OK\r\n",
        "Content-Type: application/json\r\n",
        "Content-Length: " + str(len(raw)) + "\r\n",
        "Connection: close\r\n",
        "\r\n",
        raw,
    ]
    sys.stdout.write("".join(out))

if __name__ == "__main__":
    main()
PY

"$ROOT_DIR/uwsgi_server" "$UWSGI_APP" "$PORT_UWSGI" >"$TMP_DIR/uwsgi.log" 2>&1 &
UWSGI_PID=$!

for ((i=1; i<=MAX_RETRIES; i++)); do
  if grep -q "uWSGI server listening on port $PORT_UWSGI" "$TMP_DIR/uwsgi.log" 2>/dev/null; then
    break
  fi
  sleep 0.1
done

if ! kill -0 "$UWSGI_PID" 2>/dev/null; then
  cat "$TMP_DIR/uwsgi.log"
  echo "uWSGI server failed to start" >&2
  exit 1
fi

CGI_STDOUT="$TMP_DIR/cgi_stdout.txt"
env \
  AUTH_TYPE="Basic" \
  CONTENT_LENGTH="39" \
  CONTENT_TYPE="application/json" \
  GATEWAY_INTERFACE="CGI/1.1" \
  PATH_INFO="/cgi" \
  PATH_TRANSLATED="$ROOT_DIR/spool/www/cgi" \
  QUERY_STRING="name=world" \
  REMOTE_ADDR="127.0.0.1" \
  REMOTE_HOST="localhost" \
  REMOTE_IDENT="" \
  REMOTE_USER="alice" \
  REQUEST_METHOD="POST" \
  SCRIPT_NAME="/gen_html.cgi" \
  SERVER_NAME="localhost" \
  SERVER_PORT="8080" \
  SERVER_PROTOCOL="HTTP/1.1" \
  SERVER_SOFTWARE="webserv" \
  HTTP_HOST="localhost:8080" \
  HTTP_USER_AGENT="integration-suite" \
  "$ROOT_DIR/gen_html.cgi" >"$CGI_STDOUT"

grep -q "^Content-Type: text/html; charset=UTF-8" "$CGI_STDOUT"
grep -q "^Status: 200 OK" "$CGI_STDOUT"
grep -q "CGI/1.1 Dynamic Response" "$CGI_STDOUT"
grep -q "REQUEST_METHOD</td><td>POST" "$CGI_STDOUT"
grep -q "QUERY_STRING</td><td>name=world" "$CGI_STDOUT"
grep -q "HTTP_USER_AGENT</td><td>integration-suite" "$CGI_STDOUT"

UWSGI_RESPONSE="$TMP_DIR/uwsgi_response.txt"
REQ_BODY='{"username":"alice","password":"secret"}'
python3 - "$PORT_UWSGI" "$REQ_BODY" "$UWSGI_RESPONSE" <<'PY'
import json
import socket
import sys

port, body, out_file = int(sys.argv[1]), sys.argv[2], sys.argv[3]
vars_map = {
    "REQUEST_METHOD": "POST",
    "SCRIPT_NAME": "",
    "PATH_INFO": "/uwsgi/login",
    "QUERY_STRING": "next=/home",
    "CONTENT_TYPE": "application/json",
    "CONTENT_LENGTH": str(len(body)),
    "SERVER_NAME": "localhost",
    "SERVER_PORT": "8080",
    "SERVER_PROTOCOL": "HTTP/1.1",
    "REMOTE_ADDR": "127.0.0.1",
    "HTTP_HOST": "localhost:8080",
}
vars_blob = b""
for k, v in vars_map.items():
    kb = k.encode()
    vb = v.encode()
    vars_blob += len(kb).to_bytes(2, "little") + kb
    vars_blob += len(vb).to_bytes(2, "little") + vb

# uWSGI packet format: [modifier1=0][datasize(2 bytes, little-endian)]
# [modifier2=0][vars block][request body bytes]
packet = bytes([0]) + len(vars_blob).to_bytes(2, "little") + bytes([0]) + vars_blob + body.encode()

s = socket.create_connection(("127.0.0.1", port), timeout=5)
s.sendall(packet)
s.shutdown(socket.SHUT_WR)
resp = b""
while True:
    chunk = s.recv(4096)
    if not chunk:
        break
    resp += chunk
s.close()
with open(out_file, "wb") as f:
    f.write(resp)
PY

grep -q "^HTTP/1.1 200 OK" "$UWSGI_RESPONSE"
grep -qi "^Content-Type: application/json" "$UWSGI_RESPONSE"
python3 - "$UWSGI_RESPONSE" "$REQ_BODY" <<'PY'
import json
import sys

path, expected_body = sys.argv[1], sys.argv[2]
with open(path, "rb") as f:
    raw = f.read().decode("utf-8")
_, body = raw.split("\r\n\r\n", 1)
payload = json.loads(body)
env = payload["env"]
assert payload["body"] == expected_body
assert env["REQUEST_METHOD"] == "POST"
assert env["PATH_INFO"] == "/uwsgi/login"
assert env["QUERY_STRING"] == "next=/home"
assert env["CONTENT_TYPE"] == "application/json"
assert env["CONTENT_LENGTH"] == str(len(expected_body))
assert env["SERVER_PORT"] == "8080"
assert env["REMOTE_ADDR"] == "127.0.0.1"
PY

echo "✅ CGI and uWSGI full integration suite passed"
