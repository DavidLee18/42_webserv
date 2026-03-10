# CgiDelegate and UwsgiDelegate

This document describes the two delegate classes that dispatch HTTP requests to
external programs: `CgiDelegate` (CGI/1.1 scripts) and `UwsgiDelegate` (uWSGI
servers).

---

## CgiDelegate

**Header**: `src/cgi_1_1.h`  
**Implementation**: `src/cgi_1_1.cpp`

### Purpose

`CgiDelegate` executes a CGI/1.1 script to handle an incoming HTTP request. It
forks a child process, sets up pipe-based stdin/stdout communication, passes the
parsed CGI meta-variables as environment variables, reads the script's output,
and parses it into an `Http::Response`.

### Class interface

```cpp
class CgiDelegate {
public:
  CgiDelegate(const Http::Request &req, const std::string &script);
  Result<Http::Response> execute(int timeout_ms, EPoll *epoll);
  ~CgiDelegate();
};
```

### Constructor

```cpp
CgiDelegate(const Http::Request &req, const std::string &script);
```

| Parameter | Description |
|-----------|-------------|
| `req`     | The incoming HTTP/1.1 request to dispatch to the CGI script. |
| `script`  | Path to the CGI executable. Relative paths are resolved against the server process's working directory; absolute paths are used as-is. |

The constructor calls `CgiInput::Parser::parse(req)` to build the CGI
meta-variable environment from the request. The current implementation always
succeeds and returns a fully populated `CgiInput`; the error-path guard in the
constructor is defensive code for future parser extensions.

### execute()

```cpp
Result<Http::Response> execute(int timeout_ms, EPoll *epoll);
```

| Parameter    | Description |
|--------------|-------------|
| `timeout_ms` | End-to-end request deadline in milliseconds. The wall-clock elapsed time since `execute()` was called must not exceed `timeout_ms`. Before each `EPoll::wait()` call the remaining time is recomputed and passed; if the deadline has already expired the operation returns a timeout error immediately. Pass `<= 0` for no deadline (each `EPoll::wait()` call blocks indefinitely). |
| `epoll`      | Non-NULL pointer to the server's `EPoll` instance. Used for non-blocking pipe I/O via `add_fd` / `wait`. |

**Execution steps**:

1. Creates two anonymous pipes — one for stdin (request body → child) and one
   for stdout (CGI output → parent).
2. Calls `fork()`. The child process:
   - Redirects `stdin`/`stdout` to the respective pipe ends.
   - Converts the `CgiInput` meta-variables to a `char **envp` array via
     `CgiInput::to_envp()`.
   - Calls `execve(script, argv, envp)` to run the script.
3. The parent process:
   - If the request body is non-empty, registers the write end of the stdin
     pipe with EPoll and writes the body in a loop, respecting `timeout_ms`.
   - Registers the read end of the stdout pipe with EPoll and reads the CGI
     output until EOF, respecting `timeout_ms`.
   - Calls `waitpid()` and checks the exit status.
4. Parses the CGI output: headers are separated from the body by a blank line
   (`\r\n\r\n` or `\n\n`). The optional `Status` header sets the HTTP status
   code (defaults to 200).

**Return value**: `Result<Http::Response>` — on success holds the parsed
response; on failure holds a non-empty error string. Failure causes include:

- `epoll` is NULL.
- `pipe()` or `fork()` system call failure.
- EPoll add/wait failure or timeout waiting for writability/readability.
- CGI script exits with a non-zero status.
- Read error on the stdout pipe.

### CGI output format

CGI scripts must write headers to `stdout` followed by a blank line and then the
response body. At minimum, a `Content-Type` header is required (per RFC 3875
§6.2.1).

```
Content-Type: text/html; charset=UTF-8\r\n
Status: 200 OK\r\n
\r\n
<html>...</html>
```

The `Status` header (e.g. `Status: 404 Not Found`) controls the HTTP response
status code returned to the client. If absent, the status defaults to 200.

### Environment variables passed to the script

`CgiInput::Parser::parse` populates all 17 standard CGI/1.1 meta-variables
defined by RFC 3875 §4.1:

| Variable            | Source / Value                                              | Always set? |
|---------------------|-------------------------------------------------------------|-------------|
| `AUTH_TYPE`         | Scheme from `Authorization` header (`Basic`, `Digest`, …)  | Only when `Authorization` header is present |
| `CONTENT_LENGTH`    | `Content-Length` request header                            | Only when header is present |
| `CONTENT_TYPE`      | `Content-Type` request header                              | Only when header is present |
| `GATEWAY_INTERFACE` | Hard-coded `CGI/1.1`                                       | ✅ Yes |
| `PATH_INFO`         | Full request path (up to `?`), same as `SCRIPT_NAME`. RFC 3875 §4.1.5 defines PATH_INFO as the extra path after the script name, but since this server does not split the URI into script-name vs extra-path, both variables receive the full path. | ✅ Yes |
| `PATH_TRANSLATED`   | Empty string — document root is not available here         | ✅ Yes (empty) |
| `QUERY_STRING`      | Query part of the request path (after `?`)                 | Only when query string is present |
| `REMOTE_ADDR`       | Hard-coded `127.0.0.1` — actual client IP not available    | ✅ Yes |
| `REMOTE_HOST`       | Hard-coded `localhost` (same as `REMOTE_ADDR`)             | ✅ Yes |
| `REMOTE_IDENT`      | Empty string — RFC 1413 identification not implemented     | ✅ Yes (empty) |
| `REMOTE_USER`       | Empty string — credential parsing not implemented          | Only when `Authorization` header is present |
| `REQUEST_METHOD`    | HTTP method (`GET`, `POST`, etc.)                          | ✅ Yes |
| `SCRIPT_NAME`       | Full request path (up to `?`), split on `/` into path segments | ✅ Yes |
| `SERVER_NAME`       | Hostname from `Host` header; defaults to `localhost`       | ✅ Yes |
| `SERVER_PORT`       | Port from `Host` header; defaults to `80`                  | ✅ Yes |
| `SERVER_PROTOCOL`   | Hard-coded `HTTP/1.1`                                      | ✅ Yes |
| `SERVER_SOFTWARE`   | Hard-coded `webserv`                                       | ✅ Yes |

HTTP request headers (other than `Content-Type`, `Content-Length`, and
`Authorization`) are forwarded as `HTTP_*` variables (e.g.,
`HTTP_ACCEPT_ENCODING`). Extra meta-variables can be added programmatically via
`CgiInput::add_mvar()`.

### Configuration syntax

> **Note**: The `$<script>` delegate syntax shown in `default.wbsrv` comments
> is **not yet implemented** by the server configuration parser
> (`ServerConfig`). Route rules currently require exactly four space-separated
> tokens in the form `METHOD PATH OPERATOR TARGET` (e.g.
> `GET /path <- /root/*`). `CgiDelegate` is invoked programmatically — see the
> usage example below.

### Usage example

```cpp
#include "cgi_1_1.h"
#include "epoll_kqueue.h"

// Inside a request handler that has access to an EPoll instance:
CgiDelegate delegate(request, "/usr/lib/cgi-bin/hello.cgi");
Result<Http::Response> result = delegate.execute(30000, epoll);
if (!result.error().empty()) {
    // Handle error
} else {
    Http::Response response = result.value();
    // Send response to client
}
```

---

## UwsgiDelegate

**Header**: `src/uwsgi.h`  
**Implementation**: `src/uwsgi.cpp`

### Purpose

`UwsgiDelegate` forwards an HTTP request to a running uWSGI application server
over a TCP connection. It encodes WSGI/CGI environment variables using the
[uWSGI binary protocol](https://uwsgi-docs.readthedocs.io/en/latest/Protocol.html),
sends the packet together with the request body to the server, receives the raw
HTTP response, and parses it into an `Http::Response`.

### Class interface

```cpp
class UwsgiDelegate {
public:
  UwsgiDelegate(const Http::Request &req, int uwsgi_port);
  Result<Http::Response> execute(int timeout_ms, EPoll *epoll);
  ~UwsgiDelegate();
};
```

### Constructor

```cpp
UwsgiDelegate(const Http::Request &req, int uwsgi_port);
```

| Parameter     | Description |
|---------------|-------------|
| `req`         | The incoming HTTP/1.1 request to forward to the uWSGI server. |
| `uwsgi_port`  | TCP port the uWSGI application server listens on. The host is always `127.0.0.1` (loopback). |

The constructor calls `UwsgiInput::Parser::parse(req)` to build the WSGI/CGI
environment from the request. The current implementation always succeeds:
it hard-codes `SERVER_NAME=localhost`, `SERVER_PORT=8080`,
`REMOTE_ADDR=127.0.0.1`, and several WSGI defaults, then supplements them
with values derived from the request. The uWSGI server is always contacted
at `127.0.0.1:<uwsgi_port>`.

### execute()

```cpp
Result<Http::Response> execute(int timeout_ms, EPoll *epoll);
```

| Parameter    | Description |
|--------------|-------------|
| `timeout_ms` | End-to-end request deadline in milliseconds. The same `remaining_ms` accounting used by `CgiDelegate` applies: the wall-clock elapsed time since `execute()` was called must not exceed `timeout_ms`. Before each `EPoll::wait()` call the remaining time is recomputed and passed; if the deadline has already expired the operation returns a timeout error immediately. Pass `<= 0` for no deadline. |
| `epoll`      | Non-NULL pointer to the server's `EPoll` instance. All TCP socket I/O (connect, send, receive) is performed non-blocking via `add_fd` / `wait`. |

**Execution steps**:

1. Calls `UwsgiInput::to_map()` to obtain the CGI/HTTP variables as a
   `std::map<std::string, std::string>`. WSGI-specific keys (`wsgi.version`,
   `wsgi.url_scheme`, etc.) are excluded because the uWSGI server rejects them.
2. Serialises the request body to a string (supports `Html`, `HttpJson`, and
   `HttpFormUrlEncoded` body types).
3. Encodes the uwsgi binary protocol request (4-byte header + vars block + body)
   into a single send buffer.
4. Creates a non-blocking TCP socket, sets it non-blocking, and calls
   `connect()` (expecting `EINPROGRESS`). Adds the socket to `epoll` and waits
   for writability (connect completion), verifying success via `getsockopt(SO_ERROR)`.
5. Sends the full request buffer using epoll-backed non-blocking `write()` calls,
   then performs a `shutdown(SHUT_WR)` so the server sees EOF on the request stream.
6. Switches the socket to read-interest in epoll and reads the raw HTTP response
   until EOF, using non-blocking `read()` calls.
7. Parses the raw HTTP output: headers separated from the body by a blank line
   (`\r\n\r\n` or `\n\n`). The `Status` header controls the response status
   code (defaults to 200).

**Return value**: `Result<Http::Response>` — on success holds the parsed
response; on failure holds a non-empty error string. Failure causes include:

- DNS resolution, socket creation, or non-blocking connect error.
- `epoll` add/wait failure, write error, or read error.
- Timeout (end-to-end deadline exceeded at any stage).
- The server returns an empty response.

### Wire protocol

`execute()` encodes the request as follows (all integers are little-endian):

```
[ modifier1  : 1 byte  ]  always 0 (Python/WSGI sub-type)
[ datasize   : 2 bytes ]  byte length of the vars block
[ modifier2  : 1 byte  ]  always 0 (default sub-protocol)
[ vars block : datasize bytes ]
[ body       : CONTENT_LENGTH bytes, if present ]
```

The vars block is a sequence of key/value pairs:

```
[ key_len : 2 bytes LE ][ key : key_len bytes ]
[ val_len : 2 bytes LE ][ val : val_len bytes ]
```

After sending the header, vars block, and body, the client performs a half-close
(`shutdown(SHUT_WR)`) so the server sees EOF on the request stream, then reads
the raw HTTP response until the server closes the connection.

### Variables forwarded to the uWSGI server

The following CGI/HTTP variables are serialised into the uWSGI packet:

| Variable          | Example value       |
|-------------------|---------------------|
| `REQUEST_METHOD`  | `POST`              |
| `SCRIPT_NAME`     | *(empty)*           |
| `PATH_INFO`       | `/login`            |
| `QUERY_STRING`    | `redirect=/home`    |
| `CONTENT_TYPE`    | `application/json`  |
| `CONTENT_LENGTH`  | `38`                |
| `SERVER_NAME`     | `localhost`         |
| `SERVER_PORT`     | `8080`              |
| `SERVER_PROTOCOL` | `HTTP/1.1`          |
| `REMOTE_ADDR`     | `127.0.0.1`         |
| `HTTP_*`          | *(forwarded HTTP request headers)* |

WSGI-only keys (`wsgi.version`, `wsgi.url_scheme`, `wsgi.input`,
`wsgi.errors`, `wsgi.multithread`, `wsgi.multiprocess`, `wsgi.run_once`) are
**not** included in the uWSGI packet.

### Configuration syntax

> **Note**: The `$<port>` delegate syntax shown in `default.wbsrv` comments
> is **not yet implemented** by the server configuration parser
> (`ServerConfig`). Route rules currently require exactly four space-separated
> tokens in the form `METHOD PATH OPERATOR TARGET`. `UwsgiDelegate` is invoked
> programmatically — see the usage example below.

### Usage example

```cpp
#include "uwsgi.h"

// Inside a request handler:
UwsgiDelegate delegate(request, 9000);
Result<Http::Response> result = delegate.execute(0, NULL);
if (!result.error().empty()) {
    // Handle error
} else {
    Http::Response response = result.value();
    // Send response to client
}
```

---

## Comparison

| Feature                        | CgiDelegate                              | UwsgiDelegate                              |
|-------------------------------|------------------------------------------|--------------------------------------------|
| Transport                     | stdin/stdout over anonymous pipes        | TCP socket (uWSGI binary protocol)         |
| Script language                | Any executable (C, Python, shell, …)     | Python WSGI application via uWSGI server   |
| Process model                 | Forks a new process per request          | Connects to a long-running server process  |
| EPoll integration              | Required (non-blocking pipe I/O)         | Required (non-blocking TCP socket I/O)     |
| Timeout (`timeout_ms`)         | End-to-end deadline                      | End-to-end deadline                        |
| Config syntax (planned)        | `$<script_path>` (not yet in parser)     | `$<port>` (not yet in parser)              |
