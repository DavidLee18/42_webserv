#!/usr/bin/env zsh

CGI_DIR=${CGI_DIR:-www-files/cgi-bin}

mkdir -p $CGI_DIR

if [[ -t 1 ]]; then
  C_PASS=$'\e[32m'; C_FAIL=$'\e[31m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_PASS=''; C_FAIL=''; C_DIM=''; C_OFF=''
fi

if [[ ! -d $CGI_DIR ]]; then
  print -r -- "${C_FAIL}ERROR${C_OFF} CGI_DIR=$CGI_DIR does not exist"
  print -r -- "${C_DIM}  create the directory and ensure your default.wbsrv routes it${C_OFF}"
  exit 2
fi

cat > "$CGI_DIR/hello.cgi" <<'PY'
#!/usr/bin/env python3
print("Content-Type: text/plain")
print()
print("hello from cgi")
PY

cat > "$CGI_DIR/sleep.cgi" <<'PY'
#!/usr/bin/env python3
import sys, time
print("Content-Type: text/plain")
print()
sys.stdout.flush()
time.sleep(30)
print("woke up")
PY

cat > "$CGI_DIR/burncpu.cgi" <<'PY'
#!/usr/bin/env python3
import sys
print("Content-Type: text/plain")
print()
sys.stdout.flush()
while True:
    pass
PY

cat > "$CGI_DIR/memhog.cgi" <<'PY'
#!/usr/bin/env python3
import sys
print("Content-Type: text/plain")
print()
sys.stdout.flush()
chunks = []
try:
    for _ in range(4096000000):
        chunks.append(b'X' * (1024 * 1024))
except MemoryError:
    print("memory error caught")
PY

cat > "$CGI_DIR/fbomb.cgi" <<'PY'
#!/usr/bin/env python3
import sys, tempfile
print("Content-Type: text/plain")
print()
sys.stdout.flush()
try:
    with tempfile.NamedTemporaryFile(delete=True) as f:
        chunk = b'X' * (1024 * 1024)
        for _ in range(4096000000):
            f.write(chunk)
        print("done")
except Exception as e:
    print(f"err: {e}")
PY

cat > "$CGI_DIR/fdhog.cgi" <<'PY'
#!/usr/bin/env python3
import sys
print("Content-Type: text/plain")
print()
sys.stdout.flush()
fds = []
try:
    for _ in range(200000000):
        fds.append(open('/dev/null', 'r'))
except OSError as e:
    print(f"opened {len(fds)} fds, then: {e}")
PY

cat > "$CGI_DIR/envdump.cgi" <<'PY'
#!/usr/bin/env python3
import os
print("Content-Type: text/plain")
print()
for k in sorted(os.environ):
    print(f"{k}={os.environ[k]}")
PY

cat > "$CGI_DIR/cwd.cgi" <<'PY'
#!/usr/bin/env python3
import os
print("Content-Type: text/plain")
print()
print(f"CWD={os.getcwd()}")
print("LISTING:")
try:
    for f in sorted(os.listdir('.'))[:20]:
        print(f"  {f}")
except Exception as e:
    print(f"  err: {e}")
PY

cat > "$CGI_DIR/fddump.cgi" <<'PY'
#!/usr/bin/env python3
import os
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

cat > "$CGI_DIR/echo.cgi" <<'CGI'
#!/usr/bin/env python3
import sys
sys.stdout.write("Content-Type: application/octet-stream\r\n\r\n")
sys.stdout.flush()
sys.stdout.buffer.write(sys.stdin.buffer.read())
CGI

chmod +x "$CGI_DIR"/*.cgi 2>/dev/null
