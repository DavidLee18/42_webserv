#include "uwsgi_client.h"
#include "webserv.h"

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
    : env(req), _uwsgi_port(uwsgi_port), request(req) {
  Result<UwsgiInput> env_result = UwsgiInput::Parser::parse(req);
  if (env_result.error().empty()) {
    env = env_result.value();
  }
}

Result<Http::Response> UwsgiDelegate::execute(int timeout_ms, EPoll *epoll) {
  (void)timeout_ms;
  (void)epoll;

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

  // Send request to uwsgi_server and receive its raw HTTP output
  UwsgiClient client("127.0.0.1", _uwsgi_port);
  Result<std::string> resp_result = client.send(vars, body_str);
  if (!resp_result.error().empty())
    return ERR(Http::Response, resp_result.error());

  const std::string &output = resp_result.value();
  if (output.empty())
    return ERR(Http::Response, "Empty response from uwsgi server");

  // Parse WSGI output to extract headers and body
  // WSGI scripts output headers followed by blank line, then body
  std::string headers_section;
  std::string body_section;
  size_t blank_line_pos = output.find("\r\n\r\n");

  if (blank_line_pos == std::string::npos) {
    blank_line_pos = output.find("\n\n");
    if (blank_line_pos != std::string::npos) {
      headers_section = output.substr(0, blank_line_pos);
      body_section = output.substr(blank_line_pos + 2);
    } else {
      // No headers separator found, treat all as body
      body_section = output;
    }
  } else {
    headers_section = output.substr(0, blank_line_pos);
    body_section = output.substr(blank_line_pos + 4);
  }

  // Parse headers
  std::map<std::string, std::string> response_headers;
  int status_code = 200; // Default status

  if (!headers_section.empty()) {
    std::istringstream header_stream(headers_section);
    std::string line;
    while (std::getline(header_stream, line)) {
      // Remove trailing \r if present
      if (!line.empty() && line[line.length() - 1] == '\r') {
        line = line.substr(0, line.length() - 1);
      }

      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::string header_name = line.substr(0, colon_pos);
        std::string header_value = line.substr(colon_pos + 1);

        // Trim leading whitespace from value
        size_t value_start = header_value.find_first_not_of(" \t");
        if (value_start != std::string::npos) {
          header_value = header_value.substr(value_start);
        }

        // Check for Status header
        if (header_name == "Status") {
          // Parse status code from value (e.g., "404 Not Found")
          std::istringstream status_stream(header_value);
          status_stream >> status_code;
        }

        // Store header as string (HTTP/1.1 standard)
        response_headers[header_name] = header_value;
      }
    }
  }

  // Create Http::Body from body section
  Http::Body::Value body_val;
  body_val.html_raw = new std::string(body_section);
  Http::Body result_body(Http::Body::Html, body_val);

  // Create Http::Response
  Http::Response response(status_code, response_headers, result_body);

  return OK(Http::Response, response);
}

UwsgiDelegate::~UwsgiDelegate() {
  // env is now a value member, will be automatically destroyed
}
