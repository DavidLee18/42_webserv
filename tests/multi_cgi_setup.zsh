#!/usr/bin/env zsh

emulate -L zsh
set -u

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
CGI_DIR=${CGI_DIR:-./www-files/cgi-bin}
PY_URL=${PY_URL:-/multi-cgi/echo.py}
PHP_URL=${PHP_URL:-/multi-cgi/echo.php}

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
# Deploy CGI scripts
# -----------------------------------------------------------------------------

if [[ ! -d $CGI_DIR ]]; then
  print -- "${C_FAIL}ERROR${C_OFF} CGI_DIR=$CGI_DIR does not exist"
  exit 2
fi

if (( PY_AVAIL )); then
  cat > "$CGI_DIR/echo.py" <<'PY'
import os, sys, platform
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("INTERPRETER=python\n")
sys.stdout.write(f"PYTHON_VERSION={platform.python_version()}\n")
sys.stdout.write(f"QUERY_STRING={os.environ.get('QUERY_STRING','')}\n")
sys.stdout.write(f"CONTENT_TYPE={os.environ.get('CONTENT_TYPE','')}\n")
sys.stdout.write(f"CONTENT_LENGTH={os.environ.get('CONTENT_LENGTH','')}\n")
sys.stdout.write(f"REQUEST_METHOD={os.environ.get('REQUEST_METHOD','')}\n")
try:
    cl = int(os.environ.get('CONTENT_LENGTH','0') or '0')
except ValueError:
    cl = 0
body = sys.stdin.read(cl) if cl > 0 else ''
sys.stdout.write(f"BODY={body}\n")
PY
  print -- "${C_DIM}deployed: $CGI_DIR/echo.py${C_OFF}"
fi

if (( PHP_AVAIL )); then
  cat > "$CGI_DIR/echo.php" <<'PHP'
<?php
header("Content-Type: text/plain");
echo "INTERPRETER=php\n";
echo "PHP_VERSION=" . phpversion() . "\n";
echo "QUERY_STRING=" . getenv('QUERY_STRING') . "\n";
echo "CONTENT_TYPE=" . getenv('CONTENT_TYPE') . "\n";
echo "CONTENT_LENGTH=" . getenv('CONTENT_LENGTH') . "\n";
echo "REQUEST_METHOD=" . getenv('REQUEST_METHOD') . "\n";
$cl = intval(getenv('CONTENT_LENGTH') ?: 0);
$body = $cl > 0 ? file_get_contents('php://input') : '';
echo "BODY=" . $body . "\n";
PHP
  print -- "${C_DIM}deployed: $CGI_DIR/echo.php${C_OFF}"
fi
