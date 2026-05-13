#include "ServerConfig.hpp"

ServerConfig::ServerConfig(FileDescriptor &file, std::map<std::string, std::string> &global_cgi) {
  err_meg = "";
  server_response_time = 3;
  end_flag = 0;
  count_line = 0;
  for (std::map<std::string, std::string>::const_iterator it = global_cgi.begin(); it != global_cgi.end(); ++it)
    file_extension.push_back("." + it->first);
  file_extension.push_back(".cgi");
  if (!parse_server_block(file)) {
    return;
  }
  return;
}

bool ServerConfig::parse_server_block(FileDescriptor &fd) {
  std::string line;

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

    line = utils::remove_char(temp.value(), '\n');
    err_meg = utils::get_indent_whitespace_error(line, 1);
    if (err_meg != "")
      return false;
    line = utils::trim_whitespace(line);

    if (is_header_block(line)) {
      if (!parse_header_entry(fd, line)) { // 수정 중
        err_meg = "Header syntax Error: " + err_meg;
        return false;
      }
    } else if (is_valid_server_response_time(line)) {
      parse_server_response_time(line);
      if (err_meg != "")
        return false;
    } else if (matches_route_rule_syntax(line)) {
      if (!parse_route_rule_block(line, fd)) {
        return false;
      }
    } else if (RouteRule_CGI::is_valid_cgi_config(line)) {
      RouteRule_CGI temp(fd, line, file_extension);
      count_line += temp.get_count_line();
      if (temp.get_err_meg() != "") {
        err_meg = temp.get_err_meg();
        return false;
      }
      R_CGI.push_back(temp);
    } else {
      err_meg =
          "on [\t" + line +
          "], []: The configuration line does not conform to the required "
          "server configuration syntax. (server configuration rule, each line "
          "must follow the defined config format specification, but the "
          "provided line does not match any valid syntax pattern).";
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

// :로 split후 size가 2개 인지 확인 -> 아니면 에러처리들 하기( : 기준 key, value 확인)
// 2개이면 0번 배열을 다시 " " 기준으로 split후에 2번 배열이 값을 utils::trim_whitespace 사용 후 값 사용
// 키를 utils::is_header_name 사용하여 키값 확인
// value들은 : 기준으로 한 split의 배열이 value 값
// value는 utils::is_header_value 사용하여 값 확인
bool ServerConfig::parse_header_entry(FileDescriptor &fd,
                                      const std::string &line) {
  std::string temp(line);
  if (temp.find(':') == std::string::npos) {
    std::size_t pos = temp.find("+<=") + 3;
    err_meg = "on [\t" + temp + "], [" + &temp[pos] + "]: Invalid header format (missing ':' separator between header name and value).";
    return false;
  } else if (utils::count_occurrences(temp, ":") != 1) {
    std::size_t pos = temp.find(":");
    pos = temp.find(":", pos);
    err_meg =
        "on [\t" + temp + "], [" + &temp[pos] +
        "]: The HTTP header contains an invalid format due to extra delimiter "
        "characters. (HTTP header structure, a header must follow the key: "
        "value format with only one : separator, but additional : characters "
        "are present, making parsing ambiguous and invalid).";
    return false;
  }
  std::vector<std::string> key_value = utils::string_split(temp, ":");
  std::string key = utils::string_split(key_value[0], " ")[2];
  if (!utils::is_header_name(key)) {
    err_meg = "on [\t" + temp + "], [" + key + "]: Invalid HTTP header name (reason: contains illegal characters or is empty; only alphanumeric characters and !#$%&'*+-.^_`|~ are allowed).";
    return false;
  }
  std::string value = utils::trim_whitespace(key_value[1]);
  if (!utils::is_header_value(value)) {
    err_meg = "on [\t" + temp + "], [" + value + "]: Invalid HTTP header value (reason: contains non-printable or control characters such as CR/LF or non-ASCII characters; only ASCII 0x20-0x7E and TAB are allowed).";
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
    temp = utils::remove_char(fd_line.value(), '\n');
    err_meg = utils::get_indent_whitespace_error(temp, 2);
    if (err_meg != "")
      return false;
    if (!utils::is_header_value(temp)) {
      err_meg = "on [\t" + temp + "], [" + temp + "]: Invalid HTTP header value (reason: contains non-printable or control characters such as CR/LF or non-ASCII characters; only ASCII 0x20-0x7E and TAB are allowed).";
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

void ServerConfig::parse_server_response_time(std::string line) {
  int data = 0;

  for (size_t i = 3; i < line.length(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i]))) {
      err_meg =
          "on [\t" + line + "], [" + &line[3] +
          "]: The response time configuration contains an invalid value type "
          "after the delimiter. (response time rule, the value after ... must "
          "consist only of numeric characters, but non-numeric characters are "
          "present, making it invalid for parsing).";
      return;
    }
    data = data * 10 + (line[i] - '0');
  }
  if (data > 900 || 0 >= data) {
    err_meg = "on [\t" + line + "], [" + &line[3] +
              "]: The response time configuration is out of the allowed range. "
              "(response time rule, the value must be between 0 and 900 "
              "inclusive, but the provided value falls outside this range).";
    return;
  }
  server_response_time = data;
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

std::string ServerConfig::parse_max_body_size(std::string line, int &maxbody) {
  size_t i = 0;
  maxbody = 0;
  for (; i < line.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i])))
      break;
    maxbody = maxbody * 10 + (line[i] - '0');
  }
  std::string res = "], [";
  if (i == 0)
    return err_meg =
               res + line +
               "]: Invalid max_body_size value syntax: the value after "
               "\"->{}\" must be a numeric string optionally followed by one "
               "of the allowed units: \"MB\", \"MiB\", \"KB\", or \"KiB\" (the "
               "max_body_size additional information value rule is violated "
               "because the provided value does not match the required format: "
               "<numeric string><optional unit>).";
  line = line.substr(i);
  if (line.empty() || line == "KB" || line == "KiB")
    ;
  else if (line == "MB")
    maxbody = maxbody * 1000;
  else if (line == "MiB")
    maxbody = maxbody * 1024;
  else
    return err_meg =
               res + line +
               "]: Invalid max_body_size value syntax: the value after "
               "\"->{}\" must be a numeric string optionally followed by one "
               "of the allowed units: \"MB\", \"MiB\", \"KB\", or \"KiB\" (the "
               "max_body_size additional information value rule is violated "
               "because the provided value does not match the required format: "
               "<numeric string><optional unit>).";
  return "";
}

std::string ServerConfig::apply_default_err_page_entry(
    const std::string &line, std::map<int, std::string> &err_map) {
  std::vector<std::string> split = utils::string_split(line, " ");

  if (split.size() != 2) {
    if (split.size() == 1)
      return "], []: Violates default error page format (must be \"! "
             "<status>:<path>\").";
    std::string res = "";
    for (std::size_t i = 2; i < split.size(); ++i)
      res += " " + split[i];
    return "], [" + res +
           "]: Violates default error page format (invalid format for \"! "
           "<status>:<path>\").";
  }
  std::string path = split[1];
  split = utils::string_split(split[1], ":");
  if (split.size() != 2 || utils::count_occurrences(path, ":") != 1) {
    std::size_t pos = line.find(':');
    std::string res = "], [";
    pos = line.find(':', pos + 1);
    return res + &line[pos] +
           "]: Violates error page mapping rule (expected \"status:path\" with "
           "no trailing ':' or extra fields).";
  }
  for (std::size_t i = 0; i < split[0].size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(split[0][i])))
      return "], [" + split[0] +
             "]: Violates status format rule (status must consist only of "
             "digits).";
  }
  if (split[0][0] != '4' && split[0][0] != '5' && split[0].size() != 3)
    return "], [" + split[0] +
           "]: Violates status range rule (status must start with 4xx or 5xx).";
  else if (utils::check_html_file(split[1]) != "")
    return "], [" + split[1] + "]: " + utils::check_html_file(split[1]);
  char *end;
  unsigned long num = std::strtoul(split[0].c_str(), &end, 10);
  err_map[static_cast<int>(num)] = split[1];

  return "";
}

bool ServerConfig::apply_route_rule_entry(
    const std::vector<Request::Method> &mets, const std::string &key_data,
    const std::string &line) {

  std::vector<std::string> rule = utils::string_split(line, " ");
  std::size_t size = rule.size();
  PathPattern key(key_data);

  if (size != 2) {
    if (size > 2) {
      std::size_t pos = line.find(" ");
      pos = line.find(" ", pos);
      err_meg = "on [\t\t" + line + "], [" + &line[pos] +
                "]: RouteRule additional information must contain exactly 2 "
                "elements (the RouteRule additional information rule is "
                "violated because the number of provided elements is not 2).";
    } else if (size == 1)
      err_meg = "on [\t\t" + line +
                "], []: RouteRule additional information must contain exactly "
                "2 elements (the RouteRule additional information rule is "
                "violated because the number of provided elements is not 2).";
    return false;
  }

  for (std::size_t i = 0; i < mets.size(); ++i) {
    std::size_t targetRouteIndex = routes.size();

    for (std::size_t j = 0; j < routes.size(); ++j) {
      if (routes[j].method == mets[i]) {
        const std::vector<std::string> &routePath = routes[j].path.get_path();
        const std::vector<std::string> &keyPath = key.get_path();
        if (routePath.size() == keyPath.size()) {
          bool exactMatch = true;
          for (std::size_t k = 0; k < routePath.size(); ++k) {
            if (routePath[k] != keyPath[k]) {
              exactMatch = false;
              break;
            }
          }
          if (exactMatch) {
            targetRouteIndex = j;
            break;
          }
        }
      }
    }

    if (targetRouteIndex == routes.size()) {
      RouteRule newRoute;
      newRoute.method = mets[i];
      newRoute.path = key;
      newRoute.op = UNDEFINED;
      newRoute.max_body_KB = 1;
      routes.push_back(newRoute);
    }

    if (rule[0] == "?") {
      err_meg = utils::check_html_file(rule[1]);
      if (err_meg != "") {
        err_meg = "on [\t\t" + line + "], [" + rule[1] + "]: " + err_meg;
        return false;
      }
      routes[targetRouteIndex].index = rule[1];
    } else if (rule[0] == "@") {
      char cwd[4096];
      getcwd(cwd, sizeof(cwd));

      std::string real_path = std::string(cwd) + "/" + rule[1];
      if (access(real_path.c_str(), F_OK) != 0) {
        err_meg = "on [\t\t" + line + "], [" + rule[1] + "]: the value after \"@\" must refer to an existing file (the \"@\" keyword file path rule is violated because the provided value does not exist or is not a valid file).";
        return false;
      }
      routes[targetRouteIndex].auth_info = rule[1];
    } else if (rule[0] == "->{}") {
      err_meg =
          parse_max_body_size(rule[1], routes[targetRouteIndex].max_body_KB);
      if (err_meg != "") {
        err_meg = "on [\t\t" + line + err_meg;
        return false;
      }
    } else if (rule[0] == "!") {
      std::string errPageLine = rule[1];
      err_meg = ServerConfig::apply_default_err_page_entry(
          line, routes[targetRouteIndex].error_pages);
      if (err_meg != "") {
        err_meg = "on [\t\t" + line + err_meg;
        return false;
      }
    } else {
      std::cout << "in" <<std::endl;
      err_meg = "on [\t\t" + line + "], [" + line +  "]: Invalid RouteRule additional information syntax: this line does not match the RouteRule additional information format (the RouteRule additional information syntax rule is violated because the line cannot be parsed as valid additional information; allowed keywords are \"!\", \"@\", \"->{}\", and \"?\").";
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
      if (root_pattern[i] == "*" || root_pattern[i] == "/*")
        root_wild++;
    }
  }
  if (path_wild != root_wild)
    return false;
  return true;
}

bool ServerConfig::create_route_rules(
    const std::vector<std::string> &data,
    const std::vector<Request::Method> &mets) {

  RouteRule route;
  std::vector<PathPattern> path_url = expand_path_pattern(data[1]);
  PathPattern root_url(data[3]);

  for (size_t i = 0; i < mets.size(); ++i) {
    route.method = mets[i];
    route.op = parse_rule_operator(data[2]);
    if (route.op == UNDEFINED) {
      err_meg = data[2] + "]: The route rule contains an undefined operator "
                          "type. (route rule, the operator used in the rule is "
                          "not registered in the supported operation set, so "
                          "it cannot be interpreted within the routing logic).";
      return false;
    }
    route.index = "";
    route.auth_info = "";
    route.max_body_KB = 1;

    for (size_t j = 0; j < path_url.size(); ++j) {
      route.path = path_url[j];
      route.root = root_url;
      if (!has_compatible_wildcards(route.path, route.root)) {
        err_meg = route.path.to_string() + ", " + route.root.to_string() +
                  "]: The route rule has a mismatch in wildcard usage around "
                  "the operator. (route rule, the number of wildcard * "
                  "occurrences in the left-hand and right-hand expressions "
                  "must match, but the provided rule has inconsistent wildcard "
                  "counts, making the mapping invalid).";
        return false;
      }
      if (route.op == REDIRECT)
        route.redirect_target = route.root;
      routes.push_back(route);
    }
  }
  return true;
}

bool ServerConfig::parse_route_rule_block(const std::string &route_line,
                                          FileDescriptor &fd) {
  std::string line;
  std::vector<Request::Method> mets;
  std::vector<std::string> route_line_data =
      utils::string_split(route_line, " ");
  std::vector<std::string> method =
      utils::string_split(route_line_data[0], "|");

  for (std::size_t i = 0; i < method.size(); ++i) {
    if (method[i] == "GET")
      mets.push_back(Request::GET);
    else if (method[i] == "POST")
      mets.push_back(Request::POST);
    else if (method[i] == "DELETE")
      mets.push_back(Request::DELETE);
  }

  if (!create_route_rules(route_line_data, mets)) {
    err_meg = "on [\t" + route_line + "], [" + err_meg;
    return false;
  }

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

    line = utils::remove_char(temp.value(), '\n');
    err_meg = utils::get_indent_whitespace_error(line, 2);
    line = utils::trim_whitespace(line);
    if (err_meg != "")
      return false;
    else if (!apply_route_rule_entry(mets, route_line_data[1], line))
      return false;
  }
  err_meg = "";
  return true;
}

RouteRule const *ServerConfig::find_route(Request::Method method,
                                          const std::string &path) const {
  PathPattern pathPattern(path);

  for (size_t i = 0; i < routes.size(); ++i) {
    if (routes[i].method == method && routes[i].path.matches(pathPattern)) {
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

std::ostream &operator<<(std::ostream &os, const PathPattern &data) {
  os << utils::debug << data.to_string();
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
  else
    return ("SERVEFROM (<-)");
}

std::ostream &operator<<(std::ostream &os, const ServerConfig &data) {
  os << utils::debug
     << "Server Response Time(s): " << data.get_server_response_time()
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
      std::map<int, std::string>::const_iterator err_it;
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
  if (!route)
    return "";
  return normalize_slashes(route->path.rewrite_path(path, route->root));
}
