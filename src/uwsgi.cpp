#include "uwsgi_client.h"
#include "webserv.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/time.h>
#include <vector>

// UwsgiMetaVar implementation

UwsgiMetaVar::UwsgiMetaVar(const UwsgiMetaVar &other)
    : name(other.name), value(other.value) {}

UwsgiMetaVar &UwsgiMetaVar::operator=(const UwsgiMetaVar &other) {
  if (this != &other) {
    name = other.name;
    value = other.value;
  }
  return *this;
}

UwsgiMetaVar::~UwsgiMetaVar() {}

UwsgiMetaVar UwsgiMetaVar::create(Name n, std::string v) {
  return UwsgiMetaVar(n, v);
}

// UwsgiInput implementation

UwsgiInput::UwsgiInput()
    : mvars(), req_body(Http::Body::Empty, Http::Body::Value()) {}

UwsgiInput::UwsgiInput(std::vector<UwsgiMetaVar> vars, Http::Body body)
    : mvars(vars), req_body(body) {}

UwsgiInput::UwsgiInput(Http::Request const &req)
    : mvars(), req_body(req.body()) {}

void UwsgiInput::add_mvar(std::string const &name, std::string const &value) {
  UwsgiMetaVar::Name var_name;

  if (name == "REQUEST_METHOD") {
    var_name = UwsgiMetaVar::REQUEST_METHOD;
  } else if (name == "SCRIPT_NAME") {
    var_name = UwsgiMetaVar::SCRIPT_NAME;
  } else if (name == "PATH_INFO") {
    var_name = UwsgiMetaVar::PATH_INFO;
  } else if (name == "QUERY_STRING") {
    var_name = UwsgiMetaVar::QUERY_STRING;
  } else if (name == "CONTENT_TYPE") {
    var_name = UwsgiMetaVar::CONTENT_TYPE;
  } else if (name == "CONTENT_LENGTH") {
    var_name = UwsgiMetaVar::CONTENT_LENGTH;
  } else if (name == "SERVER_NAME") {
    var_name = UwsgiMetaVar::SERVER_NAME;
  } else if (name == "SERVER_PORT") {
    var_name = UwsgiMetaVar::SERVER_PORT;
  } else if (name == "SERVER_PROTOCOL") {
    var_name = UwsgiMetaVar::SERVER_PROTOCOL;
  } else if (name == "REMOTE_ADDR") {
    var_name = UwsgiMetaVar::REMOTE_ADDR;
  } else if (name == "WSGI_VERSION") {
    var_name = UwsgiMetaVar::WSGI_VERSION;
  } else if (name == "WSGI_URL_SCHEME") {
    var_name = UwsgiMetaVar::WSGI_URL_SCHEME;
  } else if (name == "WSGI_MULTITHREAD") {
    var_name = UwsgiMetaVar::WSGI_MULTITHREAD;
  } else if (name == "WSGI_MULTIPROCESS") {
    var_name = UwsgiMetaVar::WSGI_MULTIPROCESS;
  } else if (name == "WSGI_RUN_ONCE") {
    var_name = UwsgiMetaVar::WSGI_RUN_ONCE;
  } else {
    // For HTTP headers, store as "NAME=value" in the value field
    var_name = UwsgiMetaVar::HTTP_;
    std::string combined = name + "=" + value;
    mvars.push_back(UwsgiMetaVar::create(var_name, combined));
    return;
  }

  mvars.push_back(UwsgiMetaVar::create(var_name, value));
}

char **UwsgiInput::to_envp() const {
  size_t count = mvars.size();
  char **envp = new char *[count + 1];

  for (size_t i = 0; i < count; i++) {
    const UwsgiMetaVar &mvar = mvars[i];
    std::string name;

    switch (mvar.get_name()) {
    case UwsgiMetaVar::REQUEST_METHOD:
      name = "REQUEST_METHOD";
      break;
    case UwsgiMetaVar::SCRIPT_NAME:
      name = "SCRIPT_NAME";
      break;
    case UwsgiMetaVar::PATH_INFO:
      name = "PATH_INFO";
      break;
    case UwsgiMetaVar::QUERY_STRING:
      name = "QUERY_STRING";
      break;
    case UwsgiMetaVar::CONTENT_TYPE:
      name = "CONTENT_TYPE";
      break;
    case UwsgiMetaVar::CONTENT_LENGTH:
      name = "CONTENT_LENGTH";
      break;
    case UwsgiMetaVar::SERVER_NAME:
      name = "SERVER_NAME";
      break;
    case UwsgiMetaVar::SERVER_PORT:
      name = "SERVER_PORT";
      break;
    case UwsgiMetaVar::SERVER_PROTOCOL:
      name = "SERVER_PROTOCOL";
      break;
    case UwsgiMetaVar::REMOTE_ADDR:
      name = "REMOTE_ADDR";
      break;
    case UwsgiMetaVar::WSGI_VERSION:
      name = "wsgi.version";
      break;
    case UwsgiMetaVar::WSGI_URL_SCHEME:
      name = "wsgi.url_scheme";
      break;
    case UwsgiMetaVar::WSGI_INPUT:
      name = "wsgi.input";
      break;
    case UwsgiMetaVar::WSGI_ERRORS:
      name = "wsgi.errors";
      break;
    case UwsgiMetaVar::WSGI_MULTITHREAD:
      name = "wsgi.multithread";
      break;
    case UwsgiMetaVar::WSGI_MULTIPROCESS:
      name = "wsgi.multiprocess";
      break;
    case UwsgiMetaVar::WSGI_RUN_ONCE:
      name = "wsgi.run_once";
      break;
    case UwsgiMetaVar::HTTP_:
      // For HTTP headers, the value contains "NAME=value" format
      {
        std::string env_str = mvar.get_value();
        envp[i] = new char[env_str.length() + 1];
        std::strcpy(envp[i], env_str.c_str());
        continue;
      }
    }

    std::string env_str = name + "=" + mvar.get_value();
    envp[i] = new char[env_str.length() + 1];
    std::strcpy(envp[i], env_str.c_str());
  }

  envp[count] = NULL;
  return envp;
}

// Returns the CGI/HTTP vars as a map suitable for uwsgi binary encoding.
// WSGI-specific vars (wsgi.version, wsgi.url_scheme, etc.) are excluded because
// they are not part of the CGI/HTTP namespace and the uwsgi_server rejects
// them.
std::map<std::string, std::string> UwsgiInput::to_map() const {
  std::map<std::string, std::string> result;
  for (size_t i = 0; i < mvars.size(); ++i) {
    const UwsgiMetaVar &mvar = mvars[i];
    std::string key;
    switch (mvar.get_name()) {
    case UwsgiMetaVar::REQUEST_METHOD:
      key = "REQUEST_METHOD";
      break;
    case UwsgiMetaVar::SCRIPT_NAME:
      key = "SCRIPT_NAME";
      break;
    case UwsgiMetaVar::PATH_INFO:
      key = "PATH_INFO";
      break;
    case UwsgiMetaVar::QUERY_STRING:
      key = "QUERY_STRING";
      break;
    case UwsgiMetaVar::CONTENT_TYPE:
      key = "CONTENT_TYPE";
      break;
    case UwsgiMetaVar::CONTENT_LENGTH:
      key = "CONTENT_LENGTH";
      break;
    case UwsgiMetaVar::SERVER_NAME:
      key = "SERVER_NAME";
      break;
    case UwsgiMetaVar::SERVER_PORT:
      key = "SERVER_PORT";
      break;
    case UwsgiMetaVar::SERVER_PROTOCOL:
      key = "SERVER_PROTOCOL";
      break;
    case UwsgiMetaVar::REMOTE_ADDR:
      key = "REMOTE_ADDR";
      break;
    case UwsgiMetaVar::HTTP_: {
      // value already contains "HTTP_HEADER_NAME=value"
      const std::string &combined = mvar.get_value();
      size_t eq = combined.find('=');
      if (eq != std::string::npos)
        result[combined.substr(0, eq)] = combined.substr(eq + 1);
      continue;
    }
    default:
      // Skip WSGI-specific vars (wsgi.version, wsgi.url_scheme, etc.)
      continue;
    }
    result[key] = mvar.get_value();
  }
  return result;
}

Result<UwsgiInput> UwsgiInput::Parser::parse(Http::Request const &req) {
  UwsgiInput input(req);

  // Add standard WSGI environment variables
  input.add_mvar("WSGI_VERSION", "(1, 0)");
  input.add_mvar("WSGI_URL_SCHEME", "http");
  input.add_mvar("WSGI_MULTITHREAD", "False");
  input.add_mvar("WSGI_MULTIPROCESS", "True");
  input.add_mvar("WSGI_RUN_ONCE", "True");

  // Add request method
  switch (req.method()) {
  case Http::GET:
    input.add_mvar("REQUEST_METHOD", "GET");
    break;
  case Http::POST:
    input.add_mvar("REQUEST_METHOD", "POST");
    break;
  case Http::PUT:
    input.add_mvar("REQUEST_METHOD", "PUT");
    break;
  case Http::DELETE:
    input.add_mvar("REQUEST_METHOD", "DELETE");
    break;
  case Http::HEAD:
    input.add_mvar("REQUEST_METHOD", "HEAD");
    break;
  case Http::OPTIONS:
    input.add_mvar("REQUEST_METHOD", "OPTIONS");
    break;
  case Http::TRACE:
    input.add_mvar("REQUEST_METHOD", "TRACE");
    break;
  case Http::CONNECT:
    input.add_mvar("REQUEST_METHOD", "CONNECT");
    break;
  case Http::PATCH:
    input.add_mvar("REQUEST_METHOD", "PATCH");
    break;
  }

  // Add path info
  const std::string &path = req.path();
  std::string path_str = path;

  // Parse query string from path if present
  std::string query_str;
  size_t query_pos = path.find('?');
  if (query_pos != std::string::npos) {
    query_str = path.substr(query_pos + 1);
    // Update path info to not include query string
    path_str = path.substr(0, query_pos);
  }

  input.add_mvar("PATH_INFO", path_str);
  input.add_mvar("SCRIPT_NAME", "");
  input.add_mvar("QUERY_STRING", query_str);

  // Add server info
  input.add_mvar("SERVER_NAME", "localhost");
  input.add_mvar("SERVER_PORT", "8080");
  input.add_mvar("SERVER_PROTOCOL", "HTTP/1.1");

  // Add remote address
  input.add_mvar("REMOTE_ADDR", "127.0.0.1");

  // Add content type and length
  const std::map<std::string, std::string> &headers = req.headers();

  for (std::map<std::string, std::string>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    if (it->first == "Content-Type") {
      input.add_mvar("CONTENT_TYPE", it->second);
    } else if (it->first == "Content-Length") {
      input.add_mvar("CONTENT_LENGTH", it->second);
    } else {
      // Convert HTTP headers to HTTP_* format
      std::string header_name = "HTTP_" + it->first;
      for (size_t i = 0; i < header_name.length(); i++) {
        if (header_name[i] == '-') {
          header_name[i] = '_';
        } else if (header_name[i] >= 'a' && header_name[i] <= 'z') {
          header_name[i] = static_cast<char>(header_name[i] - 'a' + 'A');
        }
      }
      input.add_mvar(header_name, it->second);
    }
  }

  return OK(UwsgiInput, input);
}

// UwsgiDelegate implementation

UwsgiDelegate::UwsgiDelegate(const Http::Request &req, int uwsgi_port)
    : env(req), _uwsgi_port(uwsgi_port), request(req),
      _state(NOT_STARTED), _epoll(NULL), _raw_sock(-1),
      _sock_epoll(NULL), _send_buf(), _total_sent(0), _output(),
      _response(NULL), _error() {
  Result<UwsgiInput> env_result = UwsgiInput::Parser::parse(req);
  if (env_result.error().empty()) {
    env = env_result.value();
  }
}

void UwsgiDelegate::_cleanup_epoll() {
  if (_epoll != NULL && _sock_epoll != NULL) {
    _epoll->del_fd(*_sock_epoll);
    _sock_epoll = NULL;
  }
  _raw_sock = -1;
}

void UwsgiDelegate::_fail(const std::string &msg) {
  _state = FAILED;
  _error = msg;
  _cleanup_epoll();
}

// Phase 1: build the uwsgi packet, open a non-blocking TCP socket to the
// uwsgi server, and register it with the shared epoll for writability so
// the connect completion notifies through the caller's main event loop.
// epoll_wait() is NEVER called from this class.
Result<Void> UwsgiDelegate::start(EPoll *epoll) {
  if (epoll == NULL) {
    return ERR(Void, "EPoll instance required");
  }
  if (_state != NOT_STARTED) {
    return ERR(Void, "UwsgiDelegate::start() already called");
  }
  _epoll = epoll;

  // Collect CGI/HTTP vars from the parsed WSGI environment
  std::map<std::string, std::string> vars = env.to_map();

  // Serialise request body
  std::string body_str;
  const Http::Body &body = request.body();
  switch (body.type()) {
  case Http::Body::Html:
    if (body.value().html_raw != NULL)
      body_str = *body.value().html_raw;
    break;
  case Http::Body::HttpJson:
    if (body.value().json != NULL) {
      std::stringstream ss;
      Json json_copy = *body.value().json;
      ss << json_copy;
      body_str = ss.str();
    }
    break;
  case Http::Body::HttpFormUrlEncoded:
    if (body.value().form != NULL) {
      std::stringstream ss;
      const std::map<std::string, std::string> &form = *body.value().form;
      bool first = true;
      for (std::map<std::string, std::string>::const_iterator it = form.begin();
           it != form.end(); ++it) {
        if (!first)
          ss << "&";
        ss << it->first << "=" << it->second;
        first = false;
      }
      body_str = ss.str();
    }
    break;
  case Http::Body::Empty:
    break;
  }

  // Build uwsgi vars block: repeated [key_len:2B LE][key][val_len:2B LE][val]
  std::vector<unsigned char> vars_block;
  for (std::map<std::string, std::string>::const_iterator it = vars.begin();
       it != vars.end(); ++it) {
    const std::string &key = it->first;
    const std::string &val = it->second;
    unsigned short key_len = static_cast<unsigned short>(key.size());
    vars_block.push_back(static_cast<unsigned char>(key_len & 0xFF));
    vars_block.push_back(static_cast<unsigned char>((key_len >> 8) & 0xFF));
    vars_block.insert(vars_block.end(), key.begin(), key.end());
    unsigned short val_len = static_cast<unsigned short>(val.size());
    vars_block.push_back(static_cast<unsigned char>(val_len & 0xFF));
    vars_block.push_back(static_cast<unsigned char>((val_len >> 8) & 0xFF));
    vars_block.insert(vars_block.end(), val.begin(), val.end());
  }
  if (vars_block.size() > static_cast<size_t>(USHRT_MAX)) {
    _state = FAILED;
    _error = "uwsgi vars block exceeds 64 KiB limit";
    return ERR(Void, _error);
  }

  // 4-byte uwsgi header: [modifier1=0][datasize:2B LE][modifier2=0]
  unsigned short datasize = static_cast<unsigned short>(vars_block.size());
  unsigned char uwsgi_header[4];
  uwsgi_header[0] = 0;
  uwsgi_header[1] = static_cast<unsigned char>(datasize & 0xFF);
  uwsgi_header[2] = static_cast<unsigned char>((datasize >> 8) & 0xFF);
  uwsgi_header[3] = 0;

  // Build the full send buffer (header + vars_block + body) into the
  // member so it persists across handle_event() calls.
  _send_buf.clear();
  _send_buf.insert(_send_buf.end(), uwsgi_header, uwsgi_header + 4);
  _send_buf.insert(_send_buf.end(), vars_block.begin(), vars_block.end());
  _send_buf.insert(_send_buf.end(), body_str.begin(), body_str.end());

  // Create a non-blocking TCP socket and connect to 127.0.0.1:_uwsgi_port
  struct addrinfo hints, *res = NULL;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  std::ostringstream port_ss;
  port_ss << _uwsgi_port;
  if (getaddrinfo("127.0.0.1", port_ss.str().c_str(), &hints, &res) != 0) {
    _state = FAILED;
    _error = "uwsgi: failed to resolve server address";
    return ERR(Void, _error);
  }

  int raw_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (raw_sock < 0) {
    freeaddrinfo(res);
    _state = FAILED;
    _error = "uwsgi: failed to create socket";
    return ERR(Void, _error);
  }

  // Wrap so the fd is automatically closed. add_fd takes ownership.
  Result<FileDescriptor> sock_fd_res = FileDescriptor::from_raw(raw_sock);
  if (!sock_fd_res.error().empty()) {
    freeaddrinfo(res);
    close(raw_sock);
    _state = FAILED;
    _error = "uwsgi: failed to wrap socket fd";
    return ERR(Void, _error);
  }
  FileDescriptor sock_fd = sock_fd_res.value();

  Result<Void> nb_res = sock_fd.set_nonblocking();
  if (!nb_res.error().empty()) {
    freeaddrinfo(res);
    _state = FAILED;
    _error = "uwsgi: failed to set socket non-blocking";
    return ERR(Void, _error);
  }

  int conn_ret = connect(raw_sock, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);
  if (conn_ret < 0 && errno != EINPROGRESS) {
    _state = FAILED;
    _error = "uwsgi: connect failed";
    return ERR(Void, _error);
  }

  _raw_sock = raw_sock;

  // Monitor for writability (connect completion) and errors.
  const FileDescriptor *sock_fd_ptr = &sock_fd;
  Event connect_event(sock_fd_ptr, false, true, false, false, true, true);
  Option connect_option(false, false, false, false);
  Result<FileDescriptor *> add_res =
      _epoll->add_fd(sock_fd, connect_event, connect_option);
  if (!add_res.has_value()) {
    _raw_sock = -1;
    _state = FAILED;
    _error = "uwsgi: failed to add socket to epoll";
    return ERR(Void, _error);
  }
  _sock_epoll = add_res.value();

  _state = (conn_ret == 0) ? SENDING : CONNECTING;
  return OKV;
}

Result<Void> UwsgiDelegate::_switch_to_sending() {
  if (_sock_epoll == NULL)
    return ERR(Void, "uwsgi: internal state: socket not registered");
  const FileDescriptor *sock_fd_ptr = _sock_epoll;
  Event write_event(sock_fd_ptr, false, true, false, false, true, true);
  Option write_option(false, false, false, false);
  _epoll->modify_fd(*_sock_epoll, write_event, write_option);
  _state = SENDING;
  return OKV;
}

Result<Void> UwsgiDelegate::_switch_to_receiving() {
  if (_sock_epoll == NULL)
    return ERR(Void, "uwsgi: internal state: socket not registered");
  // Half-close the write side so the server sees EOF on the request.
  shutdown(_raw_sock, SHUT_WR);
  const FileDescriptor *sock_fd_ptr = _sock_epoll;
  Event read_event(sock_fd_ptr, true, false, true, false, true, true);
  Option read_option(false, false, false, false);
  _epoll->modify_fd(*_sock_epoll, read_event, read_option);
  _state = RECEIVING;
  return OKV;
}

Result<Void> UwsgiDelegate::_parse_response() {
  if (_output.empty()) {
    _state = FAILED;
    _error = "Empty response from uwsgi server";
    return ERR(Void, _error);
  }

  std::string headers_section;
  std::string body_section;
  size_t blank_line_pos = _output.find("\r\n\r\n");

  if (blank_line_pos == std::string::npos) {
    blank_line_pos = _output.find("\n\n");
    if (blank_line_pos != std::string::npos) {
      headers_section = _output.substr(0, blank_line_pos);
      body_section = _output.substr(blank_line_pos + 2);
    } else {
      body_section = _output;
    }
  } else {
    headers_section = _output.substr(0, blank_line_pos);
    body_section = _output.substr(blank_line_pos + 4);
  }

  std::map<std::string, std::string> response_headers;
  int status_code = 200;

  if (!headers_section.empty()) {
    std::istringstream header_stream(headers_section);
    std::string line;
    while (std::getline(header_stream, line)) {
      if (!line.empty() && line[line.length() - 1] == '\r') {
        line = line.substr(0, line.length() - 1);
      }
      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::string header_name = line.substr(0, colon_pos);
        std::string header_value = line.substr(colon_pos + 1);
        size_t value_start = header_value.find_first_not_of(" \t");
        if (value_start != std::string::npos) {
          header_value = header_value.substr(value_start);
        }
        if (header_name == "Status") {
          std::istringstream status_stream(header_value);
          status_stream >> status_code;
        }
        response_headers[header_name] = header_value;
      }
    }
  }

  Http::Body::Value body_val;
  body_val.html_raw = new std::string(body_section);
  Http::Body result_body(Http::Body::Html, body_val);
  delete _response;
  _response = new Http::Response(status_code, response_headers, result_body);
  return OKV;
}

// Phase 2: drive the state machine based on a single epoll event.
Result<Void> UwsgiDelegate::handle_event(const Event *ev) {
  if (_state == COMPLETE || _state == FAILED || _state == NOT_STARTED) {
    return OKV;
  }
  if (ev == NULL || ev->fd == NULL) {
    return OKV;
  }
  if (_sock_epoll == NULL || _raw_sock == -1) {
    return OKV;
  }
  if (*ev->fd != _raw_sock) {
    return OKV; // not our socket
  }

  if (_state == CONNECTING) {
    if (ev->err || ev->hup) {
      _fail("uwsgi: connect error");
      return ERR(Void, _error);
    }
    if (ev->out) {
      int sock_err = 0;
      socklen_t sock_err_len = sizeof(sock_err);
      if (getsockopt(_raw_sock, SOL_SOCKET, SO_ERROR, &sock_err,
                     &sock_err_len) == 0 &&
          sock_err == 0) {
        Result<Void> sw = _switch_to_sending();
        if (!sw.has_value()) {
          _fail(sw.error());
          return ERR(Void, _error);
        }
      } else {
        _fail("uwsgi: connect failed (SO_ERROR)");
        return ERR(Void, _error);
      }
    }
    // Already in SENDING after _switch_to_sending; fall through to try a
    // first write if the same event also reported writability.
  }

  if (_state == SENDING) {
    if (ev->err) {
      _fail("uwsgi: socket error during send");
      return ERR(Void, _error);
    }
    if (ev->hup || ev->rdhup) {
      _fail("uwsgi: connection closed by peer during send");
      return ERR(Void, _error);
    }
    if (ev->out && _total_sent < _send_buf.size()) {
      ssize_t written = write(
          _raw_sock,
          reinterpret_cast<const char *>(&_send_buf[0]) + _total_sent,
          _send_buf.size() - _total_sent);
      if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          return OKV;
        }
        _fail("uwsgi: write failed");
        return ERR(Void, _error);
      } else if (written == 0) {
        _fail("uwsgi: connection closed during send");
        return ERR(Void, _error);
      }
      _total_sent += static_cast<size_t>(written);
    }
    if (_total_sent >= _send_buf.size()) {
      Result<Void> sw = _switch_to_receiving();
      if (!sw.has_value()) {
        _fail(sw.error());
        return ERR(Void, _error);
      }
    }
    return OKV;
  }

  if (_state == RECEIVING) {
    if (ev->err && !ev->in && !ev->hup && !ev->rdhup) {
      _fail("uwsgi: socket error during receive");
      return ERR(Void, _error);
    }
    if (ev->in || ev->rdhup || ev->hup) {
      char read_buf[4096];
      ssize_t n = read(_raw_sock, read_buf, sizeof(read_buf));
      if (n > 0) {
        _output.append(read_buf, static_cast<size_t>(n));
        return OKV;
      }
      if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          return OKV;
        }
        _fail("uwsgi: read error receiving response");
        return ERR(Void, _error);
      }

      // n == 0: EOF, server finished sending.
      _cleanup_epoll();
      Result<Void> p = _parse_response();
      if (!p.has_value()) {
        return ERR(Void, _error);
      }
      _state = COMPLETE;
      return OKV;
    }
    return OKV;
  }

  return OKV;
}

// DEPRECATED: synchronous wrapper retained only so that legacy callers
// compile during migration. Internally still performs an epoll_wait, so
// it violates the single-epoll_wait constraint. New code must use
// start() + handle_event() and let the main loop own epoll_wait().
Result<Http::Response> UwsgiDelegate::execute(int timeout_ms, EPoll *epoll) {
  Result<Void> s = start(epoll);
  if (!s.has_value()) {
    return ERR(Http::Response, s.error());
  }
  while (!is_done()) {
    Result<Events> wait_result = epoll->wait(timeout_ms > 0 ? timeout_ms : -1);
    if (!wait_result.has_value()) {
      _fail("uwsgi: epoll wait failed");
      return ERR(Http::Response, _error);
    }
    Events events = wait_result.value();
    if (events.is_end()) {
      _fail("uwsgi: execution timeout");
      return ERR(Http::Response, _error);
    }
    for (; !events.is_end(); ++events) {
      Result<const Event *> ev_res = *events;
      if (!ev_res.has_value())
        continue;
      Result<Void> he = handle_event(ev_res.value());
      (void)he;
      if (is_done())
        break;
    }
  }
  return result();
}

Result<Http::Response> UwsgiDelegate::result() const {
  if (_state == FAILED) {
    return ERR(Http::Response, _error);
  }
  if (_state != COMPLETE || _response == NULL) {
    return ERR(Http::Response, "uwsgi execution not complete");
  }
  return OK(Http::Response, *_response);
}

UwsgiDelegate::~UwsgiDelegate() {
  _cleanup_epoll();
  delete _response;
  _response = NULL;
  // env is a value member, destroyed automatically
}
