#include "webserv.h"

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

UwsgiInput::UwsgiInput(std::vector<UwsgiMetaVar> vars, Request const &req)
    : _mvars(vars), _req(req) {}

UwsgiInput::UwsgiInput(Request const &req) : _mvars(), _req(req) {}

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
    _mvars.push_back(UwsgiMetaVar::create(var_name, combined));
    return;
  }

  _mvars.push_back(UwsgiMetaVar::create(var_name, value));
}

char **UwsgiInput::to_envp() const {
  size_t count = _mvars.size();
  char **envp = new char *[count + 1];

  for (size_t i = 0; i < count; i++) {
    const UwsgiMetaVar &mvar = _mvars[i];
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
        std::strncpy(envp[i], env_str.c_str(), env_str.length());
        continue;
      }
    }

    std::string env_str = name + "=" + mvar.get_value();
    envp[i] = new char[env_str.length() + 1];
    std::strncpy(envp[i], env_str.c_str(), env_str.length());
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
  for (size_t i = 0; i < _mvars.size(); ++i) {
    const UwsgiMetaVar &mvar = _mvars[i];
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

Result<UwsgiInput> UwsgiInput::Parser::parse(Request const &req) {
  UwsgiInput input(req);

  // Add standard WSGI environment variables
  input.add_mvar("WSGI_VERSION", "(1, 0)");
  input.add_mvar("WSGI_URL_SCHEME", "http");
  input.add_mvar("WSGI_MULTITHREAD", "False");
  input.add_mvar("WSGI_MULTIPROCESS", "True");
  input.add_mvar("WSGI_RUN_ONCE", "True");

  switch (req.get_method()) {
  case Request::GET:
    input.add_mvar("REQUEST_METHOD", "GET");
    break;
  case Request::POST:
    input.add_mvar("REQUEST_METHOD", "POST");
    break;
  case Request::PUT:
    input.add_mvar("REQUEST_METHOD", "PUT");
    break;
  case Request::DELETE:
    input.add_mvar("REQUEST_METHOD", "DELETE");
    break;
  case Request::HEAD:
    input.add_mvar("REQUEST_METHOD", "HEAD");
    break;
  case Request::OPTIONS:
    input.add_mvar("REQUEST_METHOD", "OPTIONS");
    break;
  case Request::TRACE:
    input.add_mvar("REQUEST_METHOD", "TRACE");
    break;
  case Request::CONNECT:
    input.add_mvar("REQUEST_METHOD", "CONNECT");
    break;
  case Request::PATCH:
    input.add_mvar("REQUEST_METHOD", "PATCH");
    break;
  default:
    return ERR(UwsgiInput, "unknown request method");
  }

  const std::string &path = req.get_path();
  std::string path_str = path;

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
  input.add_mvar("SERVER_NAME", "localhost");
  input.add_mvar("SERVER_PORT", "8080");
  input.add_mvar("SERVER_PROTOCOL", "HTTP/1.1");
  input.add_mvar("REMOTE_ADDR", "127.0.0.1");

  const std::map<std::string, std::string> &headers = req.get_headers();

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

UwsgiDelegate::UwsgiDelegate(EPoll &epoll, Request const &req)
    : _env(req), _port(0), _req(req), _epoll(epoll), _sock(NULL), _send_buf(),
      _total_sent(0), _output(), _error() {}

Result<UwsgiDelegate> UwsgiDelegate::from_req(EPoll &epoll, Request const &req,
                                              unsigned short port) {
  UwsgiDelegate del(epoll, req);
  UwsgiInput input(req);
  TRY(UwsgiDelegate, UwsgiInput, input, UwsgiInput::Parser::parse(req))
  del._env = input;
  del._port = port;
  return OK(UwsgiDelegate, del);
}

// Phase 1: build the uwsgi packet, open a non-blocking TCP socket to the
// uwsgi server, and register it with the shared epoll for writability so
// the connect completion notifies through the caller's main event loop.
// epoll_wait() is NEVER called from this class.
Result<Void> UwsgiDelegate::register_() {
  if (_sock != NULL)
    return ERR(Void, "UwsgiDelegate already started");
  std::map<std::string, std::string> vars = _env.to_map();

  const std::string &body_str = _req.get_body();

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
    _error = "uwsgi vars block exceeds 64 KiB limit";
    return ERR(Void, _error);
  }

  // 4-byte uwsgi header: [modifier1=0][datasize:2B LE][modifier2=0]
  unsigned short datasize = static_cast<unsigned short>(vars_block.size());
  unsigned char uwsgi_header[4] = {
      0, static_cast<unsigned char>(datasize & 0xFF),
      static_cast<unsigned char>((datasize >> 8) & 0xFF), 0};

  // Build the full send buffer (header + vars_block + body) into the
  // member so it persists across handle_event() calls.
  _send_buf.clear();
  _send_buf.insert(_send_buf.end(), uwsgi_header, uwsgi_header + 4);
  _send_buf.insert(_send_buf.end(), vars_block.begin(), vars_block.end());
  _send_buf.insert(_send_buf.end(), body_str.begin(), body_str.end());

  std::ostringstream oss;
  oss << _port;
  struct addrinfo *ad_info = NULL;
  Result<std::pair<FileDescriptor, struct addrinfo *> > client_res =
      FileDescriptor::socket_client_new("127.0.0.1", oss.str());
  if (client_res.has_value()) {
    _sock = const_cast<FileDescriptor *>(&client_res.value().first);
    ad_info = client_res.value().second;
  }
  Result<Void> nb_res = _sock->set_nonblocking();
  if (!nb_res.error().empty()) {
    freeaddrinfo(ad_info);
    _error = "uwsgi: failed to set socket non-blocking";
    return ERR(Void, _error);
  }

  _sock->socket_connect(ad_info);

  // Monitor for writability (connect completion) and errors.
  Result<FileDescriptor *> add_res =
      _epoll.add_fd(*_sock, Event(_sock, false, true, false, false, true, true),
                    Option(false, false, false, false));
  if (!add_res.has_value()) {
    _error = "uwsgi: failed to add socket to epoll";
    return ERR(Void, _error);
  }
  _sock = add_res.value();

  return OKV;
}

// Phase 2: drive the state machine based on a single epoll event.
Result<Void> UwsgiDelegate::handle_event(const Event *ev) {
  if (ev == NULL || ev->fd == NULL || _sock == NULL || *ev->fd != *_sock) {
    return OKV;
  }

  if (ev->err || ev->hup || ev->rdhup) {
    _error = "uwsgi: event error";
    return ERR(Void, _error);
  }
  if (ev->out && _total_sent < _send_buf.size()) {
    Result<ssize_t> written = _sock->sock_send(
        reinterpret_cast<const char *>(&_send_buf[0]) + _total_sent,
        _send_buf.size() - _total_sent);
    if (!written.has_value() || written.value() < 0) {
      _error = "uwsgi: write failed";
      return ERR(Void, _error);
    } else if (written.value() == 0) {
      _error = "uwsgi: connection closed during send";
      return ERR(Void, _error);
    }
    _total_sent += static_cast<size_t>(written.value());
    return OKV;
  }
  if (ev->in) {
    char read_buf[4096];
    Result<ssize_t> n = _sock->sock_recv(read_buf, sizeof(read_buf));
    if (n.has_value() && n.value() > 0)
      _output.append(read_buf, static_cast<size_t>(n.value()));
    else if (!n.has_value() || n.value() < 0) {
      _error = "uwsgi: recv error";
      return ERR(Void, _error);
    } else
      _complete = true;

    return OKV;
  }
  _error = "uwsgi: unreachable error. if you see this you should rearrange the "
           "code..";
  return ERR(Void, _error);
}

Result<std::string> UwsgiDelegate::poll() const {
  return !_complete ? ERR(std::string, "uwsgi execution not complete")
                    : OK(std::string, _output);
}

UwsgiDelegate::~UwsgiDelegate() {}
