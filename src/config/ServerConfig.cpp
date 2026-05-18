#include "ServerConfig.hpp"

ServerConfig::ServerConfig(FileDescriptor &file,
                           std::map<std::string, std::string> &global_cgi,
                           char **envp) {
  err_meg = "";
  server_response_time_ms = 3000;
  end_flag = 0;
  count_line = 0;
  for (std::map<std::string, std::string>::const_iterator it =
           global_cgi.begin();
       it != global_cgi.end(); ++it)
    file_extension.push_back("." + it->first);
  file_extension.push_back(".cgi");
  if (!parse_server_block(file, envp)) {
    return;
  }
  return;
}

bool ServerConfig::parse_server_block(FileDescriptor &fd, char **envp) {
  std::string line;
  bool is_route_parse = false;
  bool is_header_parse = false;
  bool is_timeout_parse = false;

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    count_line++;
    if (temp.error() != "") {
      err_meg = "FileDescriptor Error: " + temp.error();
      return false;
    } else if (temp.value() == "\n") {
      end_flag += 1;
      if (end_flag == 2)
        break;
      continue;
    } else if (temp.value() == "")
      break;
    end_flag = 0;

    origin_line = utils::remove_char(temp.value(), '\n');
    err_meg = configutils::get_indent_whitespace_error(origin_line, 1);
    if (err_meg != "")
      return false;
    line = utils::trim_whitespace(origin_line);

    if (is_header_block(line)) {
      if (is_route_parse == true) {
        err_meg = ConfigError::make(origin_line, ERR_HEADER_DEFINED_AFTER_ROUTE);
        return false;
      } else if (is_header_parse == true &&
                 (is_route_parse || is_timeout_parse)) {
        err_meg = ConfigError::make(origin_line, ERR_DUPLICATE_HEADER_BLOCK);
        return false;
      }
      if (!parse_header_entry(fd, line)) {
        return false;
      }
      is_header_parse = true;
    } else if (is_valid_server_response_time(line)) {
      if (is_route_parse == true) {
        err_meg = ConfigError::make(origin_line, ERR_TIMEOUT_DEFINED_AFTER_ROUTE);
        return false;
      } else if (is_timeout_parse == true) {
        err_meg = ConfigError::make(origin_line, ERR_DUPLICATE_SERVER_TIMEOUT);
        return false;
      }
      err_meg = configutils::string_to_unsigned_int(line.substr(3),
                                              server_response_time_ms);
      if (err_meg != "") {
        err_meg = ConfigError::make(origin_line, line.substr(3), err_meg);
        return false;
      } else if (server_response_time_ms > MAX_SERVER_RESPONSE_TIME ||
                 MIN_SERVER_RESPONSE_TIME > server_response_time_ms) {
        err_meg = ConfigError::make(origin_line, line.substr(3), ERR_INVALID_SERVER_RESPONSE_TIME);
        return false;
      }
      is_timeout_parse = true;
    } else if (matches_route_rule_syntax(line)) {
      if (!parse_route_rule_block(line, fd, envp)) {
        return false;
      }
      is_route_parse = true;
    } else if (RouteRule_CGI::is_valid_cgi_config(line)) {
      RouteRule_CGI temp(fd, line, file_extension, envp);
      count_line += temp.get_count_line();
      if (temp.get_err_meg() != "") {
        err_meg = temp.get_err_meg();
        return false;
      }
      R_CGI.push_back(temp);
      is_route_parse = true;
      end_flag += 1;
    } else {
      err_meg = ConfigError::make(origin_line, "", ERR_INVALID_SERVER_CONFIG_LINE);
      return false;
    }
  }
  return true;
}

bool ServerConfig::is_header_block(const std::string &line) {
  std::vector<std::string> temp = utils::string_split(line, " ");
  if (temp.size() < 3)
    return false;
  else if (temp[0] != "[]")
    return false;
  else if (temp[1] != "+<=")
    return false;
  else if (temp[2].length() < 1)
    return false;
  return true;
}

bool ServerConfig::parse_header_entry(FileDescriptor &fd,
                                      const std::string &line) {
  std::string temp(line);
  if (temp.find(':') == std::string::npos) {
    std::size_t pos = temp.find("+<=") + 3;
    err_meg = ConfigError::make(origin_line, temp.substr(pos), ERR_INVALID_HEADER_MISSING_COLON);
    return false;
  } else if (utils::count_occurrences(temp, ":") != 1) {
    std::size_t pos = temp.find(":");
    pos = temp.find(":", pos + 1);
    err_meg = ConfigError::make(origin_line, temp.substr(pos), ERR_INVALID_HEADER_COLON_COUNT);
    return false;
  }

  std::vector<std::string> key_value =
      utils::string_split(temp.substr(temp.find("+<=") + 4), ":");
  if (key_value.size() != 2) {
    err_meg = ConfigError::make(origin_line, ERR_INVALID_HEADER_FORMAT);
    return false;
  }
  std::string key = utils::trim_whitespace(key_value[0]);
  if (!utils::is_header_name(key)) {
    err_meg = ConfigError::make(origin_line, key, ERR_INVALID_HEADER_NAME);
    return false;
  }
  std::string value = utils::trim_whitespace(key_value[1]);
  if (!utils::is_header_value(value)) {
    err_meg = ConfigError::make(origin_line, value, ERR_INVALID_HEADER_VALUE);
    return false;
  }

  while (temp[temp.length() - 1] == ';') {
    Result<std::string> fd_line = fd.read_file_line();
    count_line++;
    if (fd_line.error() != "") {
      err_meg = "FileDescriptor Error: " + fd_line.error();
      return false;
    } else if (fd_line.value() == "\n" || fd_line.value() == "") {
      end_flag += 1;
      break;
    }

    origin_line = utils::remove_char(fd_line.value(), '\n');
    err_meg = configutils::get_indent_whitespace_error(origin_line, 2);
    if (err_meg != "")
      return false;
    temp = origin_line;

    if (!utils::is_header_value(temp)) {
      err_meg = ConfigError::make(origin_line, temp, ERR_INVALID_HEADER_VALUE);
      return false;
    }
    value += " " + utils::trim_whitespace(temp);
  }
  header[key] = utils::remove_char(value, ';');
  return true;
}

bool ServerConfig::is_valid_server_response_time(const std::string &line) {
  if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
    return (false);
  return (true);
}

bool ServerConfig::is_path_pattern_segment(const std::string &line) {
  std::size_t pos = line.find("*.");
  if (pos == std::string::npos)
    return false;
  pos = pos + 2;
  if (pos >= line.size())
    return false;
  if (line[pos] != '(' || line[line.length() - 1] != ')')
    return false;

  std::string inside = line.substr(pos + 1, line.size() - pos - 2);
  if (inside.empty())
    return false;

  std::size_t count = 0;
  std::size_t start = 0;
  while (1) {
    std::size_t pipePos = inside.find('|', start);
    std::string ext = inside.substr(start, pipePos - start);
    if (ext.empty())
      return false;
    for (std::size_t i = 0; i < ext.size(); ++i) {
      if (!std::isalnum(static_cast<unsigned char>(ext[i])))
        return false;
    }
    count++;
    if (pipePos == std::string::npos)
      break;
    start = pipePos + 1;
  }
  return count >= 2;
}

std::vector<std::string>
ServerConfig::get_pattern_candidates(const std::string &line) {
  std::vector<std::string> temp = utils::string_split(line, "*");
  std::string pattern = temp[temp.size() - 1];
  size_t l = pattern.find('(');
  size_t r = pattern.find(')', l == std::string::npos ? 0 : l + 1);

  if (l != std::string::npos && r != std::string::npos && r > l)
    pattern = pattern.substr(l + 1, r - l - 1);
  temp = utils::string_split(pattern, "|");
  return (temp);
}

std::vector<PathPattern>
ServerConfig::expand_paths_with_pattern(const std::vector<PathPattern> &paths,
                                        const std::vector<std::string> &pattern,
                                        std::size_t index) {
  std::vector<PathPattern> new_paths;
  std::string seg = paths[0].get_path()[index];
  std::string prefix = "";
  std::string suffix = "";

  new_paths.reserve(paths.size() * pattern.size());
  std::size_t pos = seg.find('(');
  prefix = seg.substr(0, pos);
  pos = seg.find(')', pos + 1);
  if (pos != std::string::npos && pos + 1 < seg.size())
    suffix = seg.substr(pos + 1);

  for (std::size_t i = 0; i < paths.size(); ++i) {
    for (std::size_t j = 0; j < pattern.size(); ++j) {
      PathPattern new_path = paths[i];
      new_path.change_path(index, prefix + pattern[j] + suffix);
      new_paths.push_back(new_path);
    }
  }
  return (new_paths);
}

std::vector<PathPattern>
ServerConfig::expand_path_pattern(const std::string &line) {
  PathPattern path(line);
  std::vector<PathPattern> paths;
  std::vector<std::string> temp = path.get_path();

  paths.push_back(path);
  for (std::size_t i = 0; i < temp.size(); ++i) {
    if (is_path_pattern_segment(temp[i])) {
      std::vector<std::string> pattern = get_pattern_candidates(temp[i]);
      paths = expand_paths_with_pattern(paths, pattern, i);
    } else
      continue;
  }
  return (paths);
}

bool ServerConfig::has_valid_wildcard_usage(const std::string &url) {
  size_t i = 0;
  int count = 0;

  for (; i < url.size(); ++i) {
    if (url[i] == '*')
      count += 1;
    else if (url[i] == '/')
      count = 0;
    if (count > 1)
      return false;
  }
  return true;
}

bool ServerConfig::matches_route_rule_syntax(const std::string &line) {
  if (line.empty())
    return false;
  if (std::isspace(static_cast<unsigned char>(line[line.size() - 1])))
    return false;
  std::vector<std::string> split = utils::string_split(line, " ");
  if (split.size() != 4 || parse_rule_operator(split[2]) == UNDEFINED ||
      !has_valid_wildcard_usage(split[1]) ||
      !has_valid_wildcard_usage(split[3]))
    return false;

  std::vector<std::string> method = utils::string_split(split[0], "|");
  for (std::size_t i = 0; i < method.size(); ++i) {
    if (method[i] == "GET")
      continue;
    else if (method[i] == "POST")
      continue;
    else if (method[i] == "DELETE")
      continue;
    else
      return false;
  }
  return true;
}

std::string ServerConfig::parse_max_body_size(std::string line,
                                              unsigned int &maxbody) {
  size_t i = 0;
  maxbody = 0;
  if (line[0] == '0') {
    if (line.size() != 1)
      return ConfigError::make(origin_line, line, ERR_INVALID_UNSIGNED_INT_LEADING_ZERO);
  }
  for (; i < line.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i])))
      break;
    maxbody = maxbody * 10 + static_cast<unsigned int>(line[i] - '0');
    if (maxbody > MAX_BODY_SIZE)
      return ConfigError::make(origin_line, line, ERR_MAX_BODY_SIZE_OUT_OF_RANGE);
  }
  if (i == 0)
    return ConfigError::make(origin_line, line, ERR_INVALID_MAX_BODY_SIZE_VALUE_SYNTAX);
  std::string temp = line.substr(i);
  if (temp.empty() || temp == "KB" || temp == "KiB")
    return "";
  else if (temp == "MB") {
    if (static_cast<std::size_t>(maxbody) * 1000 > MAX_BODY_SIZE)
      return ConfigError::make(origin_line, line, ERR_MAX_BODY_SIZE_OUT_OF_RANGE);
    maxbody = maxbody * 1000;
  } else if (temp == "MiB") {
    if (static_cast<std::size_t>(maxbody) * 1024 > MAX_BODY_SIZE)
      return ConfigError::make(origin_line, line, ERR_MAX_BODY_SIZE_OUT_OF_RANGE);
    maxbody = maxbody * 1024;
  } else
    return ConfigError::make(origin_line, line, ERR_INVALID_MAX_BODY_SIZE_VALUE_SYNTAX);
  return "";
}

std::string ServerConfig::apply_err_page_entry(const std::string &origin_line,
    const std::string &line, std::map<unsigned int, std::string> &err_map,
    char **envp) {
  std::vector<std::string> split = utils::string_split(line, " ");

  if (split.size() != 2) {
    if (split.size() == 1)
      return ConfigError::make(origin_line, "", ERR_INVALID_ERROR_PAGE_FORMAT);

    std::string extra_tokens = "";
    for (std::size_t i = 2; i < split.size(); ++i)
      extra_tokens += " " + split[i];
    return ConfigError::make(origin_line, extra_tokens, ERR_INVALID_ERROR_PAGE_FORMAT);
  }

  std::size_t pos = split[1].find(":");

  if (pos == std::string::npos || utils::count_occurrences(split[1], ":") != 1)
    return ConfigError::make(origin_line, split[1], ERR_INVALID_ERROR_PAGE_MAPPING);
  
  std::vector<std::string> key_and_value = utils::string_split(split[1], ":");

  if (key_and_value.size() != 2)
    return ConfigError::make(origin_line, split[1], ERR_INVALID_ERROR_PAGE_MAPPING);

  const std::string &status_code = key_and_value[0];
  const std::string &error_page_path = key_and_value[1];

  for (std::size_t i = 0; i < status_code.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(status_code[i]))) {
      return ConfigError::make(origin_line, status_code, ERR_INVALID_ERROR_PAGE_STATUS_CODE_FORMAT);
    }
  }
  if (status_code.size() != 3 || (status_code[0] != '4' && status_code[0] != '5'))
    return ConfigError::make(origin_line, status_code, ERR_INVALID_ERROR_PAGE_STATUS_CODE);

  unsigned int status_number = 0;
  std::string err_meg = configutils::string_to_unsigned_int(status_code, status_number);
  if (err_meg != "")
    return ConfigError::make(origin_line, status_code, err_meg);
  else if (status_number < 400 || status_number > 599)
    return ConfigError::make(origin_line, status_code, ERR_INVALID_ERROR_PAGE_STATUS_CODE_RANGE);

  err_meg = configutils::check_html_file(error_page_path, envp);
  if (err_meg != "")
    return ConfigError::make(origin_line, error_page_path, err_meg);
  
  err_map[status_number] = key_and_value[1];
  return "";
}

bool ServerConfig::apply_route_rule_entry(
    const std::string &line, std::vector<std::size_t> &route_indexes,
    char **envp) {

  std::vector<std::string> rule = utils::string_split(line, " ");
  std::size_t size = rule.size();

  if (size != 2) {
    if (size > 2) {
      std::size_t pos = line.find(" ");
      pos = line.find(" ", pos + 1);
      err_meg = ConfigError::make(origin_line, line.substr(pos), ERR_INVALID_ROUTE_RULE_ADDITIONAL_INFO_FORMAT);
    } else if (size == 1)
      err_meg = ConfigError::make(origin_line, "", ERR_INVALID_ROUTE_RULE_ADDITIONAL_INFO_FORMAT);
    return false;
  }

  for (std::size_t i = 0; i < route_indexes.size(); ++i) {
    if (rule[0] == "?") {
      err_meg = configutils::check_html_file(rule[1], envp);
      if (err_meg != "") {
        err_meg = ConfigError::make(origin_line, rule[1], err_meg);
        return false;
      }
      routes[route_indexes[i]].index = rule[1];
    } else if (rule[0] == "@") {
      std::string real_path = utils::get_env("PWD", envp) + "/" + rule[1];
      if (access(real_path.c_str(), F_OK) != 0) {
        err_meg = ConfigError::make(origin_line, rule[1], ERR_AUTH_FILE_NOT_FOUND);;
        return false;
      }
      routes[route_indexes[i]].auth_info = rule[1];
    } else if (rule[0] == "->{}") {
      err_meg = parse_max_body_size(rule[1], routes[route_indexes[i]].max_body_KB);
      if (err_meg != "")
        return false;
    } else if (rule[0] == "!") {
      std::string errPageLine = rule[1];
      err_meg = ServerConfig::apply_err_page_entry(origin_line,
          line, routes[route_indexes[i]].error_pages, envp);
      if (err_meg != "")
        return false;
    } else {
      err_meg = ConfigError::make(origin_line, line, ERR_INVALID_ROUTE_RULE_ADDITIONAL_INFO_SYNTAX);
      return false;
    }
  }
  return true;
}

RuleOperator ServerConfig::parse_rule_operator(const std::string &indicator) {
  if (indicator == "<-")
    return (SERVE_FROM);
  else if (indicator == "->")
    return (UPLOAD_TO);
  else if (indicator == "<i-")
    return (AUTOINDEX);
  else if (indicator == "=300>")
    return (MULTIPLE_CHOICES);
  else if (indicator == "=301>")
    return (REDIRECT);
  else if (indicator == "=302>")
    return (FOUND);
  else if (indicator == "=303>")
    return (SEE_OTHER);
  else if (indicator == "=304>")
    return (NOT_MODIFIED);
  else if (indicator == "=307>")
    return (TEMPORARY_REDIRECT);
  else if (indicator == "=308>")
    return (PERMANENT_REDIRECT);
  else if (indicator == "#")
    return (LOGIN_USING);
  else
    return (UNDEFINED);
}

bool ServerConfig::has_compatible_wildcards(const PathPattern &path,
                                            const PathPattern &root) {
  std::vector<std::string> path_pattern = path.get_path();
  std::vector<std::string> root_pattern = root.get_path();

  int path_wild = 0;
  int root_wild = 0;
  for (size_t i = 0; i < path_pattern.size(); ++i) {
    if (std::string::npos != path_pattern[i].find('*')) {
      path_wild++;
    }
  }
  for (size_t i = 0; i < root_pattern.size(); ++i) {
    if (std::string::npos != root_pattern[i].find('*')) {
      if (root_pattern[i] != "*" && root_pattern[i] != "/*")
        return false;
      root_wild++;
    }
  }
  if (path_wild != root_wild)
    return false;
  return true;
}

bool ServerConfig::create_route_rules(
    const std::vector<std::string> &data,
    const std::vector<Request::Method> &mets,
    std::vector<std::size_t> &createdIndexes) {

  RouteRule route;
  std::vector<PathPattern> path_url = expand_path_pattern(data[1]);
  PathPattern root_url(data[3]);

  for (size_t i = 0; i < mets.size(); ++i) {
    route.method = mets[i];
    route.op = parse_rule_operator(data[2]);
    if (route.op == UNDEFINED) {
      err_meg = ConfigError::make(origin_line, data[2], ERR_UNDEFINED_ROUTE_OPERATOR);
      return false;
    }
    route.index = "";
    route.auth_info = "";
    route.max_body_KB = 0;

    for (size_t j = 0; j < path_url.size(); ++j) {
      route.path = path_url[j];
      route.root = root_url;
      if (!has_compatible_wildcards(route.path, route.root)) {
        err_meg =ConfigError::make(origin_line, route.path.to_string() + ", " + route.root.to_string(),
            ERR_INVALID_WILDCARD_MAPPING);
        return false;
      }
      if (route.op == REDIRECT)
        route.redirect_target = route.root;
      routes.push_back(route);
      createdIndexes.push_back(routes.size() - 1);
    }
  }
  return true;
}

bool ServerConfig::parse_route_rule_block(const std::string &route_line,
                                          FileDescriptor &fd, char **envp) {
  std::string line;
  std::vector<Request::Method> mets;
  std::vector<std::string> route_line_data =
      utils::string_split(route_line, " ");
  std::vector<std::string> method =
      utils::string_split(route_line_data[0], "|");
  std::vector<std::size_t> createdIndexes;

  for (std::size_t i = 0; i < method.size(); ++i) {
    if (method[i] == "GET")
      mets.push_back(Request::GET);
    else if (method[i] == "POST")
      mets.push_back(Request::POST);
    else if (method[i] == "DELETE")
      mets.push_back(Request::DELETE);
  }

  if (!create_route_rules(route_line_data, mets, createdIndexes))
    return false;

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    count_line++;
    if (temp.error() != "") {
      err_meg = "FileDescriptor Error: " + temp.error();
      return false;
    }
    if (temp.value() == "\n" || temp.value() == "") {
      end_flag += 1;
      break;
    }

    origin_line = utils::remove_char(temp.value(), '\n');
    err_meg = configutils::get_indent_whitespace_error(origin_line, 2);
    line = utils::trim_whitespace(origin_line);
    if (err_meg != "")
      return false;
    else if (!apply_route_rule_entry(line, createdIndexes, envp))
      return false;
  }
  err_meg = "";
  return true;
}

RouteRule const *ServerConfig::find_route(Request::Method method,
                                          const std::string &path) const {
  PathPattern pathPattern(path);

  for (size_t i = 0; i < routes.size(); ++i) {
    if (((method == Request::HEAD && routes[i].method == Request::GET) ||
         routes[i].method == method) &&
        routes[i].path.matches(pathPattern)) {
      return &routes[i];
    }
  }
  return NULL;
}

RouteRule_CGI const *
ServerConfig::find_route_cgi(Request::Method method,
                             const std::string &path) const {
  PathPattern pathPattern(path);

  for (size_t i = 0; i < R_CGI.size(); ++i) {
    if (R_CGI[i].get_method() == method &&
        R_CGI[i].get_path().matches(pathPattern)) {
      return &R_CGI[i];
    }
  }
  return NULL;
}

std::string normalize_slashes(const std::string &path) {
  std::string result;
  bool prev_slash = false;

  for (std::size_t i = 0; i < path.size(); ++i) {
    if (path[i] == '/') {
      if (!prev_slash)
        result += path[i];
      prev_slash = true;
    } else {
      result += path[i];
      prev_slash = false;
    }
  }
  return result;
}

std::string ServerConfig::get_rewritten_path(Request::Method method,
                                             const std::string &path) const {
  const RouteRule *route = find_route(method, path);
  if (!route) {
    std::cout << utils::debug << "get_rewritten_path: NO ROUTE for " << path
              << std::endl;
    return "";
  }
  std::cout << utils::debug << "route.path='" << route->path.to_string()
            << "' route.root='" << route->root.to_string() << "' req_path='"
            << PathPattern(path).to_string() << "'" << std::endl;
  const std::string result = normalize_slashes(
      route->path.rewrite_path(PathPattern(path), route->root));
  std::cout << utils::debug << "rewrite_path returned: '" << result << "'"
            << std::endl;
  return result;
}

std::ostream &operator<<(std::ostream &os, const PathPattern &data) {
  os << data.to_string();
  return (os);
}

static std::string what_RuleOperator(const RuleOperator op) {
  if (op == MULTIPLE_CHOICES)
    return ("MULTIPLE_CHOICES (=300>)");
  else if (op == REDIRECT)
    return ("REDIRECT (=301>)");
  else if (op == FOUND)
    return ("FOUND (=302>)");
  else if (op == SEE_OTHER)
    return ("SEE_OTHER (=303>)");
  else if (op == NOT_MODIFIED)
    return ("NOT_MODIFIED (=304>)");
  else if (op == TEMPORARY_REDIRECT)
    return ("TEMPORARY_REDIRECT (=307>)");
  else if (op == PERMANENT_REDIRECT)
    return ("PERMANENT_REDIRECT (=308>)");
  else if (op == AUTOINDEX)
    return ("AUTOINDEX (<i-)");
  else if (op == UPLOAD_TO)
    return ("UPLOAD_TO (->)");
  else if (op == LOGIN_USING)
    return ("LOGIN_USING (#)");
  else if (op == UNDEFINED)
    return ("UNDEFINED");
  else
    return ("SERVEFROM (<-)");
}

std::ostream &operator<<(std::ostream &os, const ServerConfig &data) {
  os << utils::debug
     << "Server Response Time(ms): " << data.get_server_response_time()
     << std::endl;

  const std::map<std::string, std::string> &header = data.get_header();
  std::map<std::string, std::string>::const_iterator header_it;
  os << "\n\n\n" << utils::debug << "<<Header>>";
  for (header_it = header.begin(); header_it != header.end(); ++header_it) {
    os << "\n"
       << utils::debug << "\tkey: " << header_it->first
       << ", value: " << header_it->second << std::endl;
  }

  const std::vector<RouteRule> &routes = data.get_routes();
  os << "\n\n\n" << utils::debug << "<<Routes>>";
  for (size_t i = 0; i < routes.size(); ++i) {
    const RouteRule &route = routes[i];
    os << "\n\n" << utils::debug << "Route: ";
    if (route.method == Request::GET)
      os << "GET";
    else if (route.method == Request::POST)
      os << "POST";
    else if (route.method == Request::DELETE)
      os << "DELETE";
    os << " " << route.path << std::endl;

    os << "\n"
       << utils::debug << "\tRuleOperator: " << what_RuleOperator(route.op)
       << std::endl;
    os << utils::debug << "\tRedirect Target: " << route.redirect_target
       << std::endl;

    os << "\n" << utils::debug << "\tRoot: " << route.root << std::endl;
    os << utils::debug << "\tIndex: " << route.index << std::endl;
    os << utils::debug << "\tAuth Info: " << route.auth_info << std::endl;
    os << utils::debug << "\tMax Body(KB): " << route.max_body_KB;
    if (route.error_pages.empty())
      os << "\n"
         << utils::debug << "\tError Page: "
         << "empty map";
    else {
      std::map<unsigned int, std::string>::const_iterator err_it;
      for (err_it = route.error_pages.begin();
           err_it != route.error_pages.end(); ++err_it)
        os << "\n"
           << utils::debug << "\tError Page: " << err_it->first << " "
           << err_it->second;
    }
  }
  std::vector<std::string> file_extension = data.get_file_extension();
  os << "\n\n\n\n<<File_extension>>\n";
  os << "\textension: ";
  for (std::size_t i = 0; i < file_extension.size(); ++i) {
    os << &file_extension[i][1];
    if (i + 1 < file_extension.size())
      os << ", ";
  }

  std::vector<RouteRule_CGI> cgi = data.get_route_rule_cgi();
  os << "\n\n\n\n" << utils::debug << "<<Route CGI>>\n";
  for (std::size_t i = 0; i < cgi.size(); ++i) {
    os << utils::debug << cgi[i];
  }
  if (cgi.size() == 0)
    os << "\n" << utils::debug << "\tEmpty";
  os << "\n"
     << utils::debug
     << "========================================================";
  return (os);
}