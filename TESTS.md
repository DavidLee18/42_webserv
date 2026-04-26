# webserv — Hardening & Conformance Test Plan

**Reframing.** Without TLS, "security & content integrity" is not a confidentiality
problem; it is a *robustness, spec-compliance, and HTTP hygiene* problem. This
document captures the high-value tests for a 42 webserv evaluation, ordered by
expected impact on the grade. Each section is a checklist with brief rationale,
RFC pointers, and acceptance criteria.

Probabilities are evaluator-probing estimates, not RFC-strictness levels.

---

## Status snapshot

| Area | State |
|---|---|
| Request-parsing hardening | **38/38** on `webserv_parsing_tests.zsh`. |
| Standard HTTP security headers | Not started. |
| CGI sandboxing | Not started. |
| Content integrity (ETag / Last-Modified / Repr-Digest) | Not started. |
| Resilience under adversarial load | Not started. |

---

## 1. Request-parsing hardening &nbsp; *(highest evaluator hit-rate, ~80%)*

This is where real web servers get killed. Already largely covered by
`webserv_parsing_tests.zsh`; one item remaining plus a small follow-on.

### 1.1 Already passing
- [x] Request smuggling: reject both `Content-Length` and `Transfer-Encoding: chunked` in same message (RFC 9112 §6.1).
- [x] Reject duplicate, conflicting, comma-listed, negative, non-numeric, hex-prefixed `Content-Length`.
- [x] Reject unknown `Transfer-Encoding` codings (501).
- [x] Reject bare CR / LF / NUL in header field-values (RFC 9110 §5.5).
- [x] Reject NUL in request-target.
- [x] Reject obs-fold (deprecated header continuation).
- [x] Reject whitespace before colon, empty header name.
- [x] Cap header line length (~8 KiB) and total header count (~100).
- [x] Path traversal: literal, nested, percent-encoded, mixed-case, double-encoded `../` all rejected; backslash not a separator; `%00` rejected.
- [x] Method whitelist case-sensitive; version exact-match.
- [x] Single Host header required (RFC 9112 §3.2).

### 1.2 Outstanding
- [x] **B9 — non-hex chunk size in `Transfer-Encoding: chunked`** &nbsp; Harness `expect` relaxed to `^(400|501)$` per RFC 9112 §6.1; server returns 501 (chunked decoding unimplemented). Score: **38/38**.
- [ ] **Optional follow-on** — implement chunked decoding properly. Worth it if the evaluator probes file uploads with `Transfer-Encoding: chunked` (~50% probability). curl with stdin streaming does this.

### 1.3 Follow-on probes worth running once
- [ ] CRLF injection in path: `GET /foo%0d%0aSet-Cookie:%20evil HTTP/1.1` — must not echo decoded CRLF into response headers.
- [ ] Multiple `\r\n` in request-target.
- [ ] Header line ending in `\n` only (no `\r`) — accept or reject consistently.
- [ ] Body larger than `client_max_body_size` (config) — 413, not silent truncation.
- [ ] `Expect: 100-continue` — at minimum, do not crash; ideal: 100 then proceed, or 417.

---

## 2. Standard HTTP security response headers &nbsp; *(~60% probability of NGINX-diff comparison)*

Costs ~5 lines of config (or constants), universally beneficial. Add to every
non-CGI response.

- [ ] `X-Content-Type-Options: nosniff` — disables MIME sniffing.
- [ ] `X-Frame-Options: DENY` — clickjacking baseline.
- [ ] `Referrer-Policy: no-referrer` — privacy default.
- [ ] `Content-Security-Policy: default-src 'self'` — minimal viable CSP.
- [ ] `Strict-Transport-Security` — **omit** (no TLS context here).

### Acceptance test
```zsh
curl -sI http://127.0.0.1:8080/ | grep -E '^(X-Content-Type-Options|X-Frame-Options|Referrer-Policy|Content-Security-Policy):'
```
Expected: all four headers present.

### Edge cases to verify
- [ ] CGI responses: do **not** double-emit if the CGI script already sets one.
- [ ] Error responses (4xx/5xx) carry the headers too.
- [ ] HEAD requests: headers identical to GET.

---

## 3. CGI sandboxing &nbsp; *(highest single-class crash risk in webserv)*

42 webserv projects most commonly fail or get marked down here.

### 3.1 Process limits
- [ ] **Wall-clock timeout** on the child (e.g. 5–10 s). `alarm()` in child, or `kill()` from parent on poll timeout.
  - Test: CGI script with `sleep 30` → 504 Gateway Timeout, child reaped.
- [ ] `setrlimit(RLIMIT_CPU, …)` — bounded CPU seconds.
- [ ] `setrlimit(RLIMIT_AS, …)` — bounded virtual memory.
- [ ] `setrlimit(RLIMIT_FSIZE, …)` — bounded output file size.
- [ ] `setrlimit(RLIMIT_NOFILE, …)` — bounded open fds.
  - Test for each: a CGI that exceeds the limit returns a clean 5xx, not a hang or a server crash.

### 3.2 Environment hygiene
- [ ] Scrub env to RFC 3875 §4.1 vars only: `AUTH_TYPE`, `CONTENT_LENGTH`, `CONTENT_TYPE`, `GATEWAY_INTERFACE`, `PATH_INFO`, `PATH_TRANSLATED`, `QUERY_STRING`, `REMOTE_ADDR`, `REMOTE_HOST`, `REMOTE_IDENT`, `REMOTE_USER`, `REQUEST_METHOD`, `SCRIPT_NAME`, `SERVER_NAME`, `SERVER_PORT`, `SERVER_PROTOCOL`, `SERVER_SOFTWARE`, plus `HTTP_*`.
- [ ] **Do not leak** server-process env (`PATH`, `HOME`, `LD_*`, etc.). Test:
  ```bash
  # cgi-bin/dump-env.sh
  #!/bin/sh
  echo "Content-Type: text/plain"; echo
  env
  ```
  Response should contain only RFC 3875 vars + `HTTP_*`.

### 3.3 Filesystem context
- [ ] `chdir()` into the script's directory before `execve()` — subject explicitly requires this.
  - Test: CGI script that does `cat ./local-file.txt` in same directory works.

### 3.4 Path/argument hygiene
- [ ] Never pass user-controlled bytes as arguments to `execve()` — use script path + null-terminated arg array; do not invoke a shell.
- [ ] Reject `PATH_INFO` containing `..` after decoding.

### 3.5 Body forwarding
- [ ] POST body piped to CGI stdin; close stdin after `Content-Length` bytes.
- [ ] Server must not block waiting for CGI stdout — drain via the main `epoll` loop.
- [ ] CGI stdout > pipe buffer (64 KiB on Linux): server keeps reading without deadlock.

---

## 4. Content integrity using standard machinery &nbsp; *(~80% positive evaluator signal)*

Replaces the abandoned custom `X-Content-HMAC-SHA256` direction with mechanisms
generic HTTP clients (curl, browsers, NGINX-as-upstream) actually consume.

### 4.1 ETag + conditional GET (RFC 9110 §8.8 / §13.1.2)
- [ ] On every static-file response, emit `ETag: "<hash-or-mtime-size>"`.
  - Cheap form: `"<size>-<mtime>"` (NGINX default, weak ETag).
  - Strong form: `"<sha256-hex-prefix>"` if you want representational guarantees.
- [ ] Honour `If-None-Match` → 304 Not Modified, no body.
- [ ] Test:
  ```zsh
  ETAG=$(curl -sI http://127.0.0.1:8080/index.html | awk -F'"' '/ETag/{print $2}')
  curl -sI -H "If-None-Match: \"$ETAG\"" http://127.0.0.1:8080/index.html | head -1
  # → HTTP/1.1 304 Not Modified
  ```

### 4.2 Last-Modified + If-Modified-Since
- [ ] Emit `Last-Modified` from file `mtime` for static responses.
- [ ] Honour `If-Modified-Since` → 304 if not modified.
- [ ] Date format: RFC 7231 IMF-fixdate (`Sun, 06 Nov 1994 08:49:37 GMT`).

### 4.3 Repr-Digest (RFC 9530, optional)
- [ ] If keeping any digest header, switch from custom `X-Content-SHA256` to `Repr-Digest: sha-256=:<base64>:` (note the colons — sf-binary syntax).
- [ ] Mention only on demand; not universally expected.

---

## 5. Resilience under load and adversarial clients &nbsp; *(highest-weight subject line: "must not crash, ever")*

The single most-graded category. ~90% of crash marks live here.

### 5.1 Slow / partial clients
- [ ] **Slowloris** — open connection, send `GET / HTTP/1.1\r\n`, send one byte every 10 seconds. Server must enforce a header-read timeout (e.g. 10 s total) and close cleanly.
- [ ] **Half-open** — open, send nothing. Server must time out and reclaim fd.
- [ ] **Write-stalled peer** — receive headers, then never read response. Server must not block other clients; eventually time out the write side.

### 5.2 Resource exhaustion
- [ ] **Header bomb** — 10 MB of header bytes in single request. Already capped via C8/C9; verify behaviour at the boundary (exactly cap+1 → 400/431, not crash).
- [ ] **Body bomb** — `Content-Length: 10737418240` (10 GiB). Reject before allocation if larger than `client_max_body_size`.
- [ ] **Connection flood** — 10 000 concurrent connections opened, headers sent slowly. fd table must not exhaust; `accept()` must keep returning. Set `RLIMIT_NOFILE` appropriately and degrade gracefully (refuse new connections, do not crash).
- [ ] **Pipelined burst** — single TCP segment with N back-to-back valid requests. Server should respond to each in order. (Currently handled by the `while (!in_buffer.empty())` loop.)

### 5.3 Mid-request disconnect
- [ ] Disconnect during headers — fd closed, ClientSession reaped, no leak.
- [ ] Disconnect during body — same.
- [ ] Disconnect during response write — same; partial response not retried.
- [ ] Disconnect during CGI execution — child killed, pipes closed, response dropped, no zombie.

### 5.4 Long-running soundness
- [ ] **24 h stability run** under steady 100 RPS — RSS stable, fd count stable, no slow leak.
  - Test: `ab -n 100000 -c 50 http://127.0.0.1:8080/` and watch `RSS`/`fd` count via `ls /proc/$(pidof webserv)/fd | wc -l` every 30 s.

### 5.5 Tooling
- [ ] Each adversarial scenario above scripted in a separate zsh file under `tests/`:
  - `tests/slowloris.py` — single-client, time-spaced bytes.
  - `tests/halfopen.zsh` — open-and-hold-for-N-seconds.
  - `tests/connflood.py` — concurrent connection storm.
  - `tests/disconnect-mid.py` — staged disconnect at each phase.
- [ ] Smoke harness `tests/run-all.zsh` — runs them in sequence, captures `RSS`/`fd` count before and after, fails the run if either grew.

---

## 6. Recommended execution order
1. **Section 5.3** — mid-request disconnect tests. ~1 hour. Highest crash risk per minute spent.
2. **Section 3.1, 3.2** — CGI timeouts and env scrubbing. ~half a day. Common evaluator probe.
3. **Section 2** — security headers. ~1 hour. Cheap signal.
4. **Section 4.1, 4.2** — ETag and Last-Modified. ~half a day. Conditional GET works in browsers/curl.
5. **Section 5.1, 5.2** — slowloris and resource exhaustion. ~1 day. Hardens the "must not crash" line.
6. **Chunked decoding** if time permits — proper `Transfer-Encoding: chunked` body parser. ~half a day.

Items 1–5 are roughly the realistic scope before submission.
