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

// Returns remaining milliseconds until the deadline.
// -1 = no deadline (epoll_wait blocks indefinitely).
// 0  = deadline already passed.
static int uwsgi_remaining_ms(long long start_ms, int timeout_ms) {
  if (timeout_ms <= 0)
    return -1;
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  long long now_ms = (long long)tv_now.tv_sec * 1000 + tv_now.tv_usec / 1000;
  if (now_ms < start_ms)
    return timeout_ms;
  long long rem = (long long)timeout_ms - (now_ms - start_ms);
  return (rem > 0) ? (int)rem : 0;
}

static std::string serialize_body(const Http::Body &body) {
  std::string body_str;
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
  return body_str;
}

static Result<std::vector<unsigned char>> build_uwsgi_vars_block(
    const std::map<std::string, std::string> &vars) {
  std::vector<unsigned char> vars_block;
  for (std::map<std::string, std::string>::const_iterator it = vars.begin();
       it != vars.end(); ++it) {
    const std::string &key = it->first;
    const std::string &val = it->second;
    if (key.size() > static_cast<size_t>(USHRT_MAX))
      return ERR(std::vector<unsigned char>,
                 "uwsgi vars key exceeds 65535 bytes");
    if (val.size() > static_cast<size_t>(USHRT_MAX))
      return ERR(std::vector<unsigned char>,
                 "uwsgi vars value exceeds 65535 bytes");
    unsigned short key_len = static_cast<unsigned short>(key.size());
    vars_block.push_back(static_cast<unsigned char>(key_len & 0xFF));
    vars_block.push_back(static_cast<unsigned char>((key_len >> 8) & 0xFF));
    vars_block.insert(vars_block.end(), key.begin(), key.end());
    unsigned short val_len = static_cast<unsigned short>(val.size());
    vars_block.push_back(static_cast<unsigned char>(val_len & 0xFF));
    vars_block.push_back(static_cast<unsigned char>((val_len >> 8) & 0xFF));
    vars_block.insert(vars_block.end(), val.begin(), val.end());
  }
  if (vars_block.size() > static_cast<size_t>(USHRT_MAX))
    return ERR(std::vector<unsigned char>,
               "uwsgi vars block exceeds 64 KiB limit");
  return OK(std::vector<unsigned char>, vars_block);
}

static Result<FileDescriptor *> uwsgi_epoll_connect(
    EPoll *epoll, FileDescriptor &sock_fd, int raw_sock, long long start_ms,
    int timeout_ms) {
  const FileDescriptor *sock_fd_ptr = &sock_fd;
  Event connect_event(sock_fd_ptr, false, true, false, false, true, false);
  Option connect_option(false, false, false, false);
  Result<FileDescriptor *> add_res =
      epoll->add_fd(sock_fd, connect_event, connect_option);
  if (!add_res.has_value()) {
    return ERR(FileDescriptor *, "uwsgi: failed to add socket to epoll");
  }
  FileDescriptor *sock_epoll = add_res.value();

  bool connected = false;
  while (!connected) {
    int rem = uwsgi_remaining_ms(start_ms, timeout_ms);
    if (rem == 0) {
      epoll->del_fd(*sock_epoll);
      return ERR(FileDescriptor *, "uwsgi: timeout waiting for connect");
    }
    Result<Events> wait_res = epoll->wait(rem);
    if (!wait_res.error().empty()) {
      epoll->del_fd(*sock_epoll);
      return ERR(FileDescriptor *,
                 "uwsgi: epoll wait failed during connect");
    }
    Events events = wait_res.value();
    if (events.is_end()) {
      epoll->del_fd(*sock_epoll);
      return ERR(FileDescriptor *, "uwsgi: timeout waiting for connect");
    }
    for (; !events.is_end(); ++events) {
      Result<const Event *> ev_res = *events;
      if (!ev_res.error().empty())
        continue;
      const Event *ev = ev_res.value();
      if (*ev->fd == raw_sock) {
        if (ev->err || ev->hup) {
          epoll->del_fd(*sock_epoll);
          return ERR(FileDescriptor *, "uwsgi: connect error");
        }
        if (ev->out) {
          int sock_err = 0;
          socklen_t sock_err_len = sizeof(sock_err);
          if (getsockopt(raw_sock, SOL_SOCKET, SO_ERROR, &sock_err,
                         &sock_err_len) == 0 &&
              sock_err == 0) {
            connected = true;
          } else {
            epoll->del_fd(*sock_epoll);
            return ERR(FileDescriptor *, "uwsgi: connect failed (SO_ERROR)");
          }
        }
      }
    }
  }

  return OK(FileDescriptor *, sock_epoll);
}

static Result<Void> uwsgi_epoll_send(EPoll *epoll, FileDescriptor *sock_epoll,
                                     int raw_sock,
                                     const std::vector<unsigned char> &send_buf,
                                     long long start_ms, int timeout_ms) {
  const FileDescriptor *sock_fd_ptr = sock_epoll;
  {
    Event write_event(sock_fd_ptr, false, true, false, false, false, false);
    Option write_option(false, false, false, false);
    Result<Void> modify_res = epoll->modify_fd(*sock_epoll, write_event, write_option);
    if (!modify_res.error().empty()) {
      epoll->del_fd(*sock_epoll);
      return ERR(Void, "uwsgi: epoll modify failed during send");
    }
  }

  size_t total_sent = 0;
  while (total_sent < send_buf.size()) {
    int rem = uwsgi_remaining_ms(start_ms, timeout_ms);
    if (rem == 0) {
      epoll->del_fd(*sock_epoll);
      return ERR(Void, "uwsgi: timeout during send");
    }
    Result<Events> wait_res = epoll->wait(rem);
    if (!wait_res.error().empty()) {
      epoll->del_fd(*sock_epoll);
      return ERR(Void, "uwsgi: epoll wait failed during send");
    }
    Events events = wait_res.value();
    if (events.is_end()) {
      epoll->del_fd(*sock_epoll);
      return ERR(Void, "uwsgi: timeout during send");
    }
    bool fd_ready = false;
    for (; !events.is_end(); ++events) {
      Result<const Event *> ev_res = *events;
      if (!ev_res.error().empty())
        continue;
      const Event *ev = ev_res.value();
      if (*ev->fd == raw_sock && ev->out) {
        fd_ready = true;
        break;
      }
    }
    if (!fd_ready)
      continue;

    ssize_t written = write(
        raw_sock, reinterpret_cast<const char *>(&send_buf[0]) + total_sent,
        send_buf.size() - total_sent);
    if (written < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      epoll->del_fd(*sock_epoll);
      return ERR(Void, "uwsgi: write failed");
    } else if (written == 0) {
      epoll->del_fd(*sock_epoll);
      return ERR(Void, "uwsgi: connection closed during send");
    }
    total_sent += static_cast<size_t>(written);
  }

  return OK(Void, Void());
}

static Result<std::string> uwsgi_epoll_recv(EPoll *epoll,
                                            FileDescriptor *sock_epoll,
                                            int raw_sock, long long start_ms,
                                            int timeout_ms) {
  const FileDescriptor *sock_fd_ptr = sock_epoll;
  {
    Event read_event(sock_fd_ptr, true, false, false, false, false, false);
    Option read_option(false, false, false, false);
    Result<Void> modify_res = epoll->modify_fd(*sock_epoll, read_event, read_option);
    if (!modify_res.error().empty()) {
      epoll->del_fd(*sock_epoll);
      return ERR(std::string, "uwsgi: epoll modify failed during receive");
    }
  }

  std::string output;
  char read_buf[4096];
  while (true) {
    int rem = uwsgi_remaining_ms(start_ms, timeout_ms);
    if (rem == 0) {
      epoll->del_fd(*sock_epoll);
      return ERR(std::string, "uwsgi: timeout during receive");
    }
    Result<Events> wait_res = epoll->wait(rem);
    if (!wait_res.error().empty()) {
      epoll->del_fd(*sock_epoll);
      return ERR(std::string,
                 "uwsgi: epoll wait failed during receive");
    }
    Events events = wait_res.value();
    if (events.is_end()) {
      epoll->del_fd(*sock_epoll);
      return ERR(std::string, "uwsgi: timeout during receive");
    }
    bool fd_ready = false;
    for (; !events.is_end(); ++events) {
      Result<const Event *> ev_res = *events;
      if (!ev_res.error().empty())
        continue;
      const Event *ev = ev_res.value();
      if (*ev->fd == raw_sock && (ev->in || ev->rdhup || ev->hup)) {
        fd_ready = true;
        break;
      }
    }
    if (!fd_ready)
      continue;

    ssize_t n = read(raw_sock, read_buf, sizeof(read_buf));
    if (n > 0) {
      output.append(read_buf, static_cast<size_t>(n));
    } else if (n == 0) {
      break;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      epoll->del_fd(*sock_epoll);
      return ERR(std::string,
                 "uwsgi: read error receiving response");
    }
  }

  return OK(std::string, output);
}

static Result<Http::Response>
parse_uwsgi_response_output(const std::string &output) {
  if (output.empty())
    return ERR(Http::Response, "Empty response from uwsgi server");

  std::string headers_section;
  std::string body_section;
  size_t blank_line_pos = output.find("\r\n\r\n");

  if (blank_line_pos == std::string::npos) {
    blank_line_pos = output.find("\n\n");
    if (blank_line_pos != std::string::npos) {
      headers_section = output.substr(0, blank_line_pos);
      body_section = output.substr(blank_line_pos + 2);
    } else {
      body_section = output;
    }
  } else {
    headers_section = output.substr(0, blank_line_pos);
    body_section = output.substr(blank_line_pos + 4);
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
  Http::Response response(status_code, response_headers, result_body);
  return OK(Http::Response, response);
}

UwsgiDelegate::UwsgiDelegate(const Http::Request &req, int uwsgi_port)
    : env(req), _uwsgi_port(uwsgi_port), request(req) {
  Result<UwsgiInput> env_result = UwsgiInput::Parser::parse(req);
  if (env_result.error().empty()) {
    env = env_result.value();
  }
}

Result<Http::Response> UwsgiDelegate::execute(int timeout_ms, EPoll *epoll) {
  if (epoll == NULL) {
    return ERR(Http::Response, "EPoll instance required");
  }

  struct timeval tv_start;
  gettimeofday(&tv_start, NULL);
  long long start_ms =
      (long long)tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000;

  std::map<std::string, std::string> vars = env.to_map();

  const Http::Body &body = request.body();
  std::string body_str = serialize_body(body);

  Result<std::vector<unsigned char>> vars_block_res =
      build_uwsgi_vars_block(vars);
  if (!vars_block_res.error().empty()) {
    return ERR(Http::Response, vars_block_res.error());
  }
  std::vector<unsigned char> vars_block = vars_block_res.value();

  unsigned short datasize = static_cast<unsigned short>(vars_block.size());
  unsigned char uwsgi_header[4];
  uwsgi_header[0] = 0;
  uwsgi_header[1] = static_cast<unsigned char>(datasize & 0xFF);
  uwsgi_header[2] = static_cast<unsigned char>((datasize >> 8) & 0xFF);
  uwsgi_header[3] = 0;

  std::vector<unsigned char> send_buf;
  send_buf.insert(send_buf.end(), uwsgi_header, uwsgi_header + 4);
  send_buf.insert(send_buf.end(), vars_block.begin(), vars_block.end());
  send_buf.insert(send_buf.end(), body_str.begin(), body_str.end());

  struct addrinfo hints, *res = NULL;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  std::ostringstream port_ss;
  port_ss << _uwsgi_port;
  if (getaddrinfo("127.0.0.1", port_ss.str().c_str(), &hints, &res) != 0)
    return ERR(Http::Response, "uwsgi: failed to resolve server address");

  int raw_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (raw_sock < 0) {
    freeaddrinfo(res);
    return ERR(Http::Response, "uwsgi: failed to create socket");
  }

  Result<FileDescriptor> sock_fd_res = FileDescriptor::from_raw(raw_sock);
  if (!sock_fd_res.error().empty()) {
    freeaddrinfo(res);
    close(raw_sock);
    return ERR(Http::Response, "uwsgi: failed to wrap socket fd");
  }
  FileDescriptor sock_fd = sock_fd_res.value();

  Result<Void> nb_res = sock_fd.set_nonblocking();
  if (!nb_res.error().empty()) {
    freeaddrinfo(res);
    return ERR(Http::Response, "uwsgi: failed to set socket non-blocking");
  }

  int conn_ret = connect(raw_sock, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);
  if (conn_ret < 0 && errno != EINPROGRESS) {
    return ERR(Http::Response, "uwsgi: connect failed");
  }

  Result<FileDescriptor *> connect_res =
      uwsgi_epoll_connect(epoll, sock_fd, raw_sock, start_ms, timeout_ms);
  if (!connect_res.error().empty()) {
    return ERR(Http::Response, connect_res.error());
  }
  FileDescriptor *sock_epoll = connect_res.value();

  Result<Void> send_res =
      uwsgi_epoll_send(epoll, sock_epoll, raw_sock, send_buf, start_ms,
                       timeout_ms);
  if (!send_res.error().empty()) {
    return ERR(Http::Response, send_res.error());
  }

  shutdown(raw_sock, SHUT_WR);

  Result<std::string> recv_res =
      uwsgi_epoll_recv(epoll, sock_epoll, raw_sock, start_ms, timeout_ms);
  if (!recv_res.error().empty()) {
    return ERR(Http::Response, recv_res.error());
  }
  std::string output = recv_res.value();

  epoll->del_fd(*sock_epoll);

  Result<Http::Response> parse_res = parse_uwsgi_response_output(output);
  if (!parse_res.error().empty()) {
    return ERR(Http::Response, parse_res.error());
  }
  return OK(Http::Response, parse_res.value());
}

UwsgiDelegate::~UwsgiDelegate() {
  // env is now a value member, will be automatically destroyed
}
