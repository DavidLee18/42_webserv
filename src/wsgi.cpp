#include "webserv.h"

// WsgiMetaVar implementation

WsgiMetaVar::WsgiMetaVar(const WsgiMetaVar &other)
    : name(other.name), value(other.value) {}

WsgiMetaVar& WsgiMetaVar::operator=(const WsgiMetaVar &other) {
  if (this != &other) {
    name = other.name;
    value = other.value;
  }
  return *this;
}

WsgiMetaVar::~WsgiMetaVar() {}

WsgiMetaVar WsgiMetaVar::create(Name n, std::string v) {
  return WsgiMetaVar(n, v);
}

// WsgiInput implementation

WsgiInput::WsgiInput() : mvars(), req_body(Http::Body::Empty, Http::Body::Value()) {}

WsgiInput::WsgiInput(std::vector<WsgiMetaVar> vars, Http::Body body)
    : mvars(vars), req_body(body) {}

WsgiInput::WsgiInput(Http::Request const &req) : mvars(), req_body(req.body()) {}

void WsgiInput::add_mvar(std::string const &name, std::string const &value) {
  WsgiMetaVar::Name var_name;
  
  if (name == "REQUEST_METHOD") {
    var_name = WsgiMetaVar::REQUEST_METHOD;
  } else if (name == "SCRIPT_NAME") {
    var_name = WsgiMetaVar::SCRIPT_NAME;
  } else if (name == "PATH_INFO") {
    var_name = WsgiMetaVar::PATH_INFO;
  } else if (name == "QUERY_STRING") {
    var_name = WsgiMetaVar::QUERY_STRING;
  } else if (name == "CONTENT_TYPE") {
    var_name = WsgiMetaVar::CONTENT_TYPE;
  } else if (name == "CONTENT_LENGTH") {
    var_name = WsgiMetaVar::CONTENT_LENGTH;
  } else if (name == "SERVER_NAME") {
    var_name = WsgiMetaVar::SERVER_NAME;
  } else if (name == "SERVER_PORT") {
    var_name = WsgiMetaVar::SERVER_PORT;
  } else if (name == "SERVER_PROTOCOL") {
    var_name = WsgiMetaVar::SERVER_PROTOCOL;
  } else if (name == "REMOTE_ADDR") {
    var_name = WsgiMetaVar::REMOTE_ADDR;
  } else if (name == "WSGI_VERSION") {
    var_name = WsgiMetaVar::WSGI_VERSION;
  } else if (name == "WSGI_URL_SCHEME") {
    var_name = WsgiMetaVar::WSGI_URL_SCHEME;
  } else if (name == "WSGI_MULTITHREAD") {
    var_name = WsgiMetaVar::WSGI_MULTITHREAD;
  } else if (name == "WSGI_MULTIPROCESS") {
    var_name = WsgiMetaVar::WSGI_MULTIPROCESS;
  } else if (name == "WSGI_RUN_ONCE") {
    var_name = WsgiMetaVar::WSGI_RUN_ONCE;
  } else {
    // For HTTP headers, store as "NAME=value" in the value field
    var_name = WsgiMetaVar::HTTP_;
    std::string combined = name + "=" + value;
    mvars.push_back(WsgiMetaVar::create(var_name, combined));
    return;
  }
  
  mvars.push_back(WsgiMetaVar::create(var_name, value));
}

char **WsgiInput::to_envp() const {
  size_t count = mvars.size();
  char **envp = new char*[count + 1];
  
  for (size_t i = 0; i < count; i++) {
    const WsgiMetaVar &mvar = mvars[i];
    std::string name;
    
    switch (mvar.get_name()) {
      case WsgiMetaVar::REQUEST_METHOD:
        name = "REQUEST_METHOD";
        break;
      case WsgiMetaVar::SCRIPT_NAME:
        name = "SCRIPT_NAME";
        break;
      case WsgiMetaVar::PATH_INFO:
        name = "PATH_INFO";
        break;
      case WsgiMetaVar::QUERY_STRING:
        name = "QUERY_STRING";
        break;
      case WsgiMetaVar::CONTENT_TYPE:
        name = "CONTENT_TYPE";
        break;
      case WsgiMetaVar::CONTENT_LENGTH:
        name = "CONTENT_LENGTH";
        break;
      case WsgiMetaVar::SERVER_NAME:
        name = "SERVER_NAME";
        break;
      case WsgiMetaVar::SERVER_PORT:
        name = "SERVER_PORT";
        break;
      case WsgiMetaVar::SERVER_PROTOCOL:
        name = "SERVER_PROTOCOL";
        break;
      case WsgiMetaVar::REMOTE_ADDR:
        name = "REMOTE_ADDR";
        break;
      case WsgiMetaVar::WSGI_VERSION:
        name = "wsgi.version";
        break;
      case WsgiMetaVar::WSGI_URL_SCHEME:
        name = "wsgi.url_scheme";
        break;
      case WsgiMetaVar::WSGI_INPUT:
        name = "wsgi.input";
        break;
      case WsgiMetaVar::WSGI_ERRORS:
        name = "wsgi.errors";
        break;
      case WsgiMetaVar::WSGI_MULTITHREAD:
        name = "wsgi.multithread";
        break;
      case WsgiMetaVar::WSGI_MULTIPROCESS:
        name = "wsgi.multiprocess";
        break;
      case WsgiMetaVar::WSGI_RUN_ONCE:
        name = "wsgi.run_once";
        break;
      case WsgiMetaVar::HTTP_:
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

Result<WsgiInput> WsgiInput::Parser::parse(Http::Request const &req) {
  WsgiInput input(req);
  
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
  
  return OK(WsgiInput, input);
}

// WsgiDelegate implementation

WsgiDelegate::WsgiDelegate(const Http::Request &req, const std::string &script)
    : env(req), script_path(script), request(req) {
  Result<WsgiInput> env_result = WsgiInput::Parser::parse(req);
  if (env_result.error().empty()) {
    env = env_result.value();
  }
}

WsgiDelegate *WsgiDelegate::create(const Http::Request &req,
                                   const std::string &script) {
  WsgiDelegate *delegate = new WsgiDelegate(req, script);
  return delegate;
}

Result<Http::Response *> WsgiDelegate::execute(int timeout_ms, EPoll *epoll) {
  
  if (epoll == NULL) {
    return ERR(Http::Response *, "EPoll instance required");
  }

  // Create pipes for communication
  int stdin_pipe[2];
  int stdout_pipe[2];

  if (pipe(stdin_pipe) == -1) {
    return ERR(Http::Response *, "Failed to create stdin pipe");
  }
  if (pipe(stdout_pipe) == -1) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    return ERR(Http::Response *, "Failed to create stdout pipe");
  }

  // Fork the process
  pid_t pid = fork();
  if (pid == -1) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return ERR(Http::Response *, "Failed to fork process");
  }

  if (pid == 0) {
    // Child process

    // Close unused pipe ends
    close(stdin_pipe[1]);  // Close write end of stdin pipe
    close(stdout_pipe[0]); // Close read end of stdout pipe

    // Redirect stdin and stdout
    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
      std::cerr << "Failed to redirect stdin" << std::endl;
      exit(1);
    }
    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
      std::cerr << "Failed to redirect stdout" << std::endl;
      exit(1);
    }

    // Close the pipe file descriptors after duplication
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Prepare environment variables
    char **envp = env.to_envp();

    // Prepare arguments to execute Python with the WSGI script
    char *argv[3];
    argv[0] = const_cast<char *>("python3");
    argv[1] = const_cast<char *>(script_path.c_str());
    argv[2] = NULL;

    // Execute Python with the WSGI script
    execve("/usr/bin/python3", argv, envp);

    // If execve returns, it failed
    // Clean up allocated memory before exit
    for (size_t i = 0; envp[i] != NULL; i++) {
      delete[] envp[i];
    }
    delete[] envp;
    
    std::cerr << "Failed to execute WSGI script: " << script_path << std::endl;
    exit(1);
  }

  // Parent process

  // Close unused pipe ends
  close(stdin_pipe[0]);  // Close read end of stdin pipe
  close(stdout_pipe[1]); // Close write end of stdout pipe

  // Create FileDescriptor objects for the pipes
  Result<FileDescriptor> stdin_fd_res = FileDescriptor::from_raw(stdin_pipe[1]);
  if (!stdin_fd_res.error().empty()) {
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return ERR(Http::Response *, "Failed to create stdin FileDescriptor");
  }
  FileDescriptor stdin_fd = stdin_fd_res.value();

  Result<FileDescriptor> stdout_fd_res = FileDescriptor::from_raw(stdout_pipe[0]);
  if (!stdout_fd_res.error().empty()) {
    // stdin_pipe[1] is owned by stdin_fd, will be closed automatically
    close(stdout_pipe[0]);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return ERR(Http::Response *, "Failed to create stdout FileDescriptor");
  }
  FileDescriptor stdout_fd = stdout_fd_res.value();

  // Prepare request body for writing
  const Http::Body &body = request.body();
  std::string body_str;
  
  switch (body.type()) {
    case Http::Body::Html:
      if (body.value().html_raw != NULL) {
        body_str = *body.value().html_raw;
      }
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
          if (!first) {
            ss << "&";
          }
          ss << it->first << "=" << it->second;
          first = false;
        }
        body_str = ss.str();
      }
      break;
      
    case Http::Body::Empty:
      // No body to write
      break;
  }
  
  // Write request body to WSGI stdin if present, using EPoll to check writability
  if (!body_str.empty()) {
    // Add stdin_fd to epoll for writing
    const FileDescriptor *stdin_fd_ptr = const_cast<const FileDescriptor *>(&stdin_fd);
    Event write_event(stdin_fd_ptr, false, true, false, false, false, false);
    Option write_option(false, false, false, false);
    Result<const FileDescriptor *> add_result = epoll->add_fd(stdin_fd, write_event, write_option);
    
    if (!add_result.error().empty()) {
      // FileDescriptor destructors will close the pipes
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response *, "Failed to add stdin to epoll");
    }

    size_t total_written = 0;
    while (total_written < body_str.length()) {
      // Wait for the fd to be writable
      Result<Events> wait_result = epoll->wait(timeout_ms);
      if (!wait_result.error().empty()) {
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response *, "EPoll wait failed for stdin");
      }
      
      Events events = wait_result.value();
      
      // Check if timeout occurred (no events returned)
      if (events.is_end()) {
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response *, "Timeout waiting for stdin writability");
      }
      
      bool fd_ready = false;
      
      for (; !events.is_end(); ++events) {
        Result<const Event *> event_result = *events;
        if (!event_result.error().empty()) {
          continue;
        }
        const Event *ev = event_result.value();
        if (*ev->fd == stdin_pipe[1] && ev->out) {
          fd_ready = true;
          break;
        }
      }
      
      if (!fd_ready) {
        // Got events but not for our fd, continue waiting
        continue;
      }
      
      // FD is ready, perform write
      ssize_t written = write(stdin_pipe[1], 
                              body_str.c_str() + total_written, 
                              body_str.length() - total_written);
      if (written < 0) {
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response *, "Failed to write to WSGI stdin");
      } else if (written == 0) {
        // Pipe closed by reader (child process)
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response *, "WSGI process closed stdin prematurely");
      }
      total_written += static_cast<size_t>(written);
    }
    
    // Remove stdin from epoll and close it
    epoll->del_fd(stdin_fd);
  }
  // Close stdin by letting stdin_fd go out of scope
  // (Manual close would be a double-close bug)
  {
    // Create a scope to destroy stdin_fd and close stdin_pipe[1]
    FileDescriptor temp_fd = stdin_fd;
    // temp_fd destructor will close stdin_pipe[1]
  }

  // Add stdout_fd to epoll for reading
  const FileDescriptor *stdout_fd_ptr = const_cast<const FileDescriptor *>(&stdout_fd);
  Event read_event(stdout_fd_ptr, true, false, false, false, false, false);
  Option read_option(false, false, false, false);
  Result<const FileDescriptor *> add_stdout_result = epoll->add_fd(stdout_fd, read_event, read_option);
  
  if (!add_stdout_result.error().empty()) {
    // stdout_fd destructor will close stdout_pipe[0]
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return ERR(Http::Response *, "Failed to add stdout to epoll");
  }

  // Read output using EPoll to check readability
  std::string output;
  char buffer[4096];
  
  while (true) {
    // Wait for data to be available
    Result<Events> wait_result = epoll->wait(timeout_ms);
    if (!wait_result.error().empty()) {
      epoll->del_fd(stdout_fd);
      // stdout_fd destructor will close stdout_pipe[0]
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response *, "EPoll wait failed for stdout");
    }
    
    Events events = wait_result.value();
    
    // Check if timeout occurred (no events returned)
    if (events.is_end()) {
      epoll->del_fd(stdout_fd);
      // stdout_fd destructor will close stdout_pipe[0]
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response *, "WSGI execution timeout");
    }
    
    bool fd_ready = false;
    
    for (; !events.is_end(); ++events) {
      Result<const Event *> event_result = *events;
      if (!event_result.error().empty()) {
        continue;
      }
      const Event *ev = event_result.value();
      if (*ev->fd == stdout_pipe[0] && ev->in) {
        fd_ready = true;
        break;
      }
    }
    
    if (!fd_ready) {
      // Got events but not for our fd, continue waiting
      continue;
    }
    
    // FD is ready, perform read
    ssize_t bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
      output.append(buffer, static_cast<size_t>(bytes_read));
    } else if (bytes_read == 0) {
      // EOF reached
      break;
    } else {
      // Read error
      epoll->del_fd(stdout_fd);
      // stdout_fd destructor will close stdout_pipe[0]
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response *, "Failed to read from WSGI stdout");
    }
  }

  // Remove stdout from epoll and close it
  epoll->del_fd(stdout_fd);
  // stdout_fd destructor will close stdout_pipe[0] when it goes out of scope

  // Wait for child process
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    return ERR(Http::Response *, "Failed to wait for child process");
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return ERR(Http::Response *, "WSGI script failed");
  }

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
  Http::Response *response = new Http::Response(status_code, response_headers, result_body);

  return OK(Http::Response *, response);
}

WsgiDelegate::~WsgiDelegate() {
  // env is now a value member, will be automatically destroyed
}
