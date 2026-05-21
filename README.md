*This project has been created as part of the 42 curriculum by jaehylee, jaejo, and sipyeon.*

# webserv

## Description

`webserv` is a non-blocking HTTP/1.1 server written from scratch in C++ 98,
modelled on a small subset of NGINX's `server`-block configuration grammar.
It services many concurrent clients on a single thread via a Linux `epoll`
event loop running in edge-triggered mode, with all sockets and pipes held in
non-blocking mode.

The mandatory feature surface comprises:

- HTTP/1.1 request parsing (request-line, headers, chunked and `Content-Length`
  bodies) with strict RFC 9110/9112 conformance — invalid framing, header
  injection, oversize lines, and malformed encodings are rejected with the
  appropriate 4xx status before any handler runs.
- The `GET`, `POST`, and `DELETE` methods, plus `HEAD` as a free side-effect of
  the response pipeline.
- Static file serving with MIME-type negotiation, optional directory listing
  (`autoindex`), and a configurable default index file.
- HTTP redirection at route granularity (`301`/`302`).
- CGI/1.1 execution (RFC 3875) by file extension, with per-request
  environment isolation, `chdir` into the script's directory, full request
  body forwarded over a pipe, chunked-request un-chunking before delivery,
  and a configurable wall-clock timeout that returns `504 Gateway Timeout`
  on overrun.
- Multipart file upload to a configurable directory with a per-route
  request-body size cap.
- Cookie-based session management (mandatory-side feature; not the bonus
  scope).
- Per-listener and per-route configuration, including custom error pages
  and a single configurable response timeout that drives both slow-loris
  protection and chunked-pending eviction.

The server is designed to remain operational under abusive client behaviour:
malformed requests, mid-request disconnects, slow-loris-style trickling
clients, oversized bodies, and CGI scripts that hang or crash do not bring
down the event loop or leak file descriptors.

## Instructions

### Build

```sh
make            # builds ./webserv
make re         # full rebuild
make clean      # remove object files
make fclean     # remove object files and the binary
```

The build requires a C++ 98 compiler (`clang++` 11+ or `g++` 9+) and a Linux
host with `epoll` support. macOS and BSD targets are not supported; the
project uses Linux-only `epoll_*` syscalls.

Compilation flags: `-Wall -Wextra -Werror -std=c++98`. The build is warning-
clean on the supported toolchains.

### Run

```sh
./webserv [configuration_file]
```

If no configuration file is supplied, the server prints its usage line
and exits. The configuration grammar (file extension `.wbsrv`) is
described in `CONFIG_SYNTAX.md` at the project root; `default.wbsrv`
ships as the sole sample configuration.

Any directory referenced as an `upload_dir` in the configuration must
exist and be writable before the server is launched — `webserv` does
not create upload directories on demand. For the bundled
default.wbsrv, create the directory it references (e.g.
`mkdir -p /tmp/webserv_uploads`) before starting the server.

### Quick smoke test

```sh
./webserv configs/default.wbsrv &
curl -i http://127.0.0.1:8080/
kill %1
```

### Test suites

Custom test harnesses in `tests/` exercise parsing, mid-request disconnect
resilience, CGI sandboxing, security headers, and slowloris-class attacks.
Each harness is a self-contained `zsh` script that spawns the server, runs
its probes against it, and reports pass/fail counts.

```sh
./tests/webserv_parsing_tests.zsh
./tests/webserv_disconnect_tests.zsh
./tests/webserv_cgi_tests.zsh
./tests/webserv_headers_tests.zsh
./tests/webserv_slowloris_tests.zsh
```

## Feature list

| Category          | Feature                                                    |
| ----------------- | ---------------------------------------------------------- |
| Transport         | epoll (edge-triggered), non-blocking I/O, `SIGPIPE` masked |
| Methods           | `GET`, `POST`, `DELETE`, `HEAD`                            |
| Request framing   | `Content-Length`, chunked transfer-encoding, trailers      |
| Static            | MIME negotiation, autoindex, default index                 |
| Dynamic           | CGI/1.1 by extension, configurable interpreter map         |
| Routing           | Per-route methods, redirect, upload-dir, body-size cap     |
| Errors            | Configurable error pages per status code                   |
| Sessions          | Cookie-based, in-memory store with expiry                  |
| Security headers  | `X-Content-Type-Options`, `X-Frame-Options`,               |
|                   | `Referrer-Policy`, `Content-Security-Policy`               |
| Resilience        | Per-connection timeout, mid-request-disconnect cleanup,    |
|                   | `EPOLLRDHUP` registration, bounded CGI wall-clock          |

## Technical choices

### Concurrency model

Single-threaded, single `epoll_wait` loop in edge-triggered mode.
All client sockets, CGI pipe ends, and listening sockets register
once and are demultiplexed by an `EPoll` wrapper that distinguishes
listeners from clients from CGI delegates by the pointer stored in
the `epoll_data_t` field. CGI child reaping is fully asynchronous:
a `Reaping` state is entered on stdout EOF and one
`waitpid(WNOHANG)` per loop tick drives the child to terminal,
keeping the loop non-blocking even during CGI shutdown.

### Error handling

A `Result<T>` template (loosely modelled on Rust's) wraps every
syscall surface so error paths are total and explicit. No
exceptions cross the event-loop boundary.

### File-descriptor ownership

A move-only `FileDescriptor` class owns every raw fd; destruction
closes deterministically, including on stack unwinding. The CGI
pipe-end lifetime in particular relies on this RAII for leak-free
shutdown under all the disconnect-mid-request paths.

### Deviations from the allowed-functions list

The subject's allowed-functions list (Chapter IV) does not provide
primitives for several operations the subject itself mandates. The
following retained calls are documented here pre-emptively:

- **`unlink`** (`Response.cpp::delete_method`). The subject mandates
  the `DELETE` method, but the allowed list contains no
  file-deletion primitive (no `unlink`, no `remove`, no `rmdir`).
  The only allowed-function path is `fork` + `execve("/bin/rm",
  …)`, which spawns a process per request and introduces a
  `PATH`-dependent failure mode. `unlink` is the minimal atomic
  syscall for the mandated operation.

- **`clock_gettime(CLOCK_MONOTONIC, …)`** (Server.cpp, cgi_1_1.cpp).
  The subject mandates resilience and bounded CGI response times.
  Per-connection timeout enforcement requires a monotonic clock;
  the allowed list contains none. `time()` and `gettimeofday()`
  are wall-clock-based and jump on NTP adjustment, breaking
  timeout invariants. `clock_gettime(CLOCK_MONOTONIC)` is the
  correct POSIX primitive — monotonic, side-effect-free, and used
  by every production HTTP server for the same purpose.

- **`gmtime` + `strftime`** (`Response.cpp::get_http_date`).
  Used solely to format the HTTP `Date` response header in
  RFC 9110 §6.6.1 IMF-fixdate form. The allowed list contains no
  time-formatting primitive. Hand-rolling a Gregorian-calendar
  formatter for a single header is disproportionate; this server
  has a clock (see above), so emitting `Date` improves cache
  correctness and NGINX behavioural parity.

- **`fdopen` / `fclose` / `fgets` / `feof`**
  (`FileDescriptor::open_file`, `::read_file_line`). Used as a
  buffered line-reader on top of an `open()`-acquired fd for the
  config-file parser only — startup path, never in the event loop.
  The allowed list provides `read()` but no buffered line
  abstraction. Reimplementing one is ~150 lines on a
  submission-critical parser path with non-trivial correctness
  risk (line splits across read boundaries, partial reads, EOF
  handling). The blast radius is bounded: a single fd, opened
  once, closed once, at startup.

All four are amenable to in-spec replacement on request; the
choices above reflect a deliberate trade-off between strict
adherence and engineering risk this close to submission.

## Resources

### Specifications

- RFC 9110 — HTTP Semantics
- RFC 9112 — HTTP/1.1 (message syntax and routing)
- RFC 3875 — The Common Gateway Interface (CGI) Version 1.1
- RFC 6265 — HTTP State Management Mechanism (cookies)
- RFC 2046 — MIME Media Types (multipart/form-data, §5.1)

### Documentation and reference implementations

- `man 7 epoll`, `man 2 epoll_ctl`, `man 2 epoll_wait`
- `man 2 accept`, `man 2 send`, `man 2 recv`, `man 2 fcntl`
- The NGINX `server`-block reference grammar (configuration design only)
- Beej's Guide to Network Programming (socket API patterns)

### Use of AI

Anthropic's Claude Opus 4.7 was used throughout development in an advisory and
diagnostic role, never as a code-generation tool for unreviewed output.
Specifically:

- **Diagnosis of subtle bugs in the request-parsing and disconnect-
  handling paths** — Claude was given the failing test cases and the
  relevant source, and produced root-cause analyses (e.g. inverted
  `is_partial()` predicate, missing `peer_closed` checks on the
  body-pending branch, `SIGPIPE` handling). Fixes were authored by
  hand from those diagnostic specifications.
- **Forbidden-function audit** — Claude enumerated every call in the
  codebase outside the subject's allowed list, classified each by
  severity, and proposed replacement strategies for those we chose
  to address.
- **RFC cross-checking** — Claude was queried on specific clauses of
  RFC 9110/9112/3875 when behavioural questions arose (e.g. whether
  `Date` may be omitted; the exact grammar of a chunk extension).
  All clause references were verified against the published RFCs
  before code was changed.
- **Test-harness design review** — the `zsh` harnesses in `tests/`
  were drafted by hand and submitted to Claude for coverage-gap
  analysis and edge-case suggestions.

OpenAI's chatGPT 5.5 was used only as an auxiliary learning and review resource.

In this project, chat was used for:

- Clarifying concepts related to HTTP, CGI, sockets, request parsing, and response handling.
- Reviewing C++98 code for possible logic errors, unsafe behavior, and maintainability issues.
- Improving the clarity of error messages.
- Discussing edge cases such as path rewriting, wildcard matching, empty paths, invalid requests, and configuration parsing.
- Helping structure documentation and explanations.

This project also leveraged Google Gemini as a supplementary learning and development assistant. The AI was strictly utilized to accelerate conceptual understanding and enhance testing robustly, specifically in the following areas:

* **HTTP Protocol & Architecture:** Clarified RFC standards, request/response structures, and state management to design a compliant web server.
* **I/O Multiplexing (epoll):** Explored the core mechanics of Linux `epoll` (including interest lists, readiness events, and edge-triggered vs. level-triggered behaviors) to implement an efficient event loop.
* **Test Case Generation:** Assisted in formulating diverse and rigorous edge-case scenarios to validate the server's stability, error handling, and performance under high concurrency.
* **HTML/UI Design:** Assisted in scaffolding clean, responsive HTML/CSS templates for the server's default landing and error pages.

No source file in `src/` was machine-generated. Every line of submitted
code was authored or transcribed manually by one of the three named
contributors, who can each defend any portion of the codebase orally.
