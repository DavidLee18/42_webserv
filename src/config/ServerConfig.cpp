#include "ServerConfig.hpp"

ServerConfig::ServerConfig(FileDescriptor &file) {
  err_meg = "";
  server_response_time = 3;
  end_flag = 0;
  count_line = 0;
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
      if (!parse_header_entry(fd, line)) { // 수정해야 함
        err_meg = "Header syntax Error: " + err_meg;
        return false;
      }
    } else if (RouteRule_CGI::matches_cgi_syntax(line)) {
      std::string key;
      std::map<std::string, std::string> temp;
      err_meg = RouteRule_CGI::parse_executable(line, key, temp);
      if (err_meg != "") {
        err_meg = "on [\t" + line + err_meg;
        return false;
      }
      if (S_CGI.find(key) != S_CGI.end()) {
        err_meg = "on [\t" + line + "], [" + key + "]: The CGI server block configuration is duplicated. (CGI server block rule, each CGI path must be declared only once per server context, but the same CGI definition appears multiple times, causing a configuration conflict).";
        return false;
      }
      S_CGI[key] = temp;
    } else if (is_valid_server_response_time(line)) {
      parse_server_response_time(line);
      if (err_meg != "")
        return false;
    }
    else if (matches_route_rule_syntax(line)) {
      if (!parse_route_rule_block(line, fd)) { // 수정 중
        return false;
      }
    } else if (RouteRule_CGI::is_valid_cgi_config(line)) {
      RouteRule_CGI temp(fd, line);
      count_line += temp.get_count_line();
      if (temp.get_err_meg() != "") {
        err_meg = temp.get_err_meg();
        return false;
      }
      R_CGI.push_back(temp);
    } else {
      err_meg = "on [\t" + line + "], []: The configuration line does not conform to the required server configuration syntax. (server configuration rule, each line must follow the defined config format specification, but the provided line does not match any valid syntax pattern).";
      return false;
    }
  }
  return true;
}

// header method
bool ServerConfig::is_header_block(const std::string &line) {
  std::vector<std::string> temp = utils::string_split(line, " ");
  if (temp.size() < 4)
    return false;
  else if (temp[0] != "[]")
    return false;
  else if (temp[1] != "+<=")
    return false;
  else if (temp[2][temp[2].length() - 1] != ':')
    return false;
  else if (temp[3].length() < 1)
    return false;
  return true;
}

bool ServerConfig::parse_header_entry(FileDescriptor &fd,
                                      const std::string &line) {
  std::string temp(line);
  std::vector<std::string> key_value = utils::string_split(temp, ":");
  std::string key;
  
  if (utils::count_occurrences(temp, ":") != 1) {
    std::size_t pos = temp.find(":");
    pos = temp.find(":", pos);
    err_meg = "on [\t" + temp + "], [" + &temp[pos] + "]: The HTTP header contains an invalid format due to extra delimiter characters. (HTTP header structure, a header must follow the key: value format with only one : separator, but additional : characters are present, making parsing ambiguous and invalid).";
    return false;
  } else if (key_value.size() != 2) {
    err_meg = "The HTTP header value is missing, so the request cannot be processed. (HTTP header structure, a key must have an associated value, but it is empty, making the header invalid).";
    // if () 키가 없는 경우
      err_meg = "The HTTP header key is missing, so the request cannot be processed. (HTTP header structure, a header must include a key to be identifiable, but the key is empty).";
    return false;
  }
  key = utils::string_split(key_value[0], " ")[2];
  std::string value = utils::trim_whitespace(key_value[1]);
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
    value += " " + utils::trim_whitespace(temp);
  }
  header[key] = utils::remove_char(value, ';');
  return true;
}

// server_response_time method
bool ServerConfig::is_valid_server_response_time(const std::string &line) {
  if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
    return (false);
  return (true);
}

void ServerConfig::parse_server_response_time(std::string line) {
  int data = 0;

  for (size_t i = 3; i < line.length(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i]))) {
      err_meg = "on [\t" + line + "], [" + &line[3] + "]: The response time configuration contains an invalid value type after the delimiter. (response time rule, the value after ... must consist only of numeric characters, but non-numeric characters are present, making it invalid for parsing).";
      return ;
    }
    data = data * 10 + (line[i] - '0');
  }
  if (data > 900 || 0 >= data) {
    err_meg = "on [\t" + line + "], [" + &line[3] + "]: The response time configuration is out of the allowed range. (response time rule, the value must be between 0 and 900 inclusive, but the provided value falls outside this range).";
    return ;
  }
  server_response_time = data;
}

// RouteRule method
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

std::vector<PathPattern> ServerConfig::expand_paths_with_pattern(
    const std::vector<PathPattern> &paths,
    const std::vector<std::string> &pattern, std::size_t index) {
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
    return (false);
  if (std::isspace(static_cast<unsigned char>(line[line.size() - 1])))
    return (false);
  std::vector<std::string> split = utils::string_split(line, " ");
  if (split.size() != 4 || parse_rule_operator(split[2]) == UNDEFINED ||
      !has_valid_wildcard_usage(split[1]) ||
      !has_valid_wildcard_usage(split[3]))
    return (false);

  std::vector<std::string> method = utils::string_split(split[0], "|");
  for (std::size_t i = 0; i < method.size(); ++i) {
    if (method[i] == "GET")
      continue;
    else if (method[i] == "POST")
      continue;
    else if (method[i] == "DELETE")
      continue;
    else
      return (false);
  }
  return (true);
}

int ServerConfig::parse_max_body_size(std::string line) {
  size_t i = 0;
  int maxbody = 0;
  for (; i < line.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i])))
      break;
    maxbody = maxbody * 10 + (line[i] - '0');
  }
  if (i == 0)
    return (-1);
  line = line.substr(i);
  if (line.empty() || line == "KB" || line == "KiB")
    return (maxbody);
  else if (line == "MB")
    return (maxbody * 1000);
  else if (line == "MiB")
    return (maxbody * 1024);
  return (-1);
}

std::string ServerConfig::get_valid_index_file(const std::string &line) {
  size_t i = 0;
  for (; i < line.size(); ++i) {
    if (line[i] == '.')
      break;
  }
  std::string extension = line.substr(i);
  if (extension.empty())
    return ("");
  else if (extension == ".html" || extension == ".htm")
    return (line);
  else
    return ("");
}

int ServerConfig::parse_error_page_entry(std::string &line) {
  int key = 0;
  std::vector<std::string> key_and_value = utils::string_split(line, ":");

  if (key_and_value.size() != 2)
    return (0);
  for (std::size_t i = 0; i < key_and_value[0].size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(key_and_value[0][i])))
      return (0);
    key = key * 10 + (key_and_value[0][i] - '0');
  }
  line = key_and_value[1];
  return (key);
}

bool ServerConfig::apply_route_rule_entry(
    const std::vector<Request::Method> &mets, const std::string &key_data,
    const std::string &line) {
  if (line.empty())
    return false;
  if (std::isspace(static_cast<unsigned char>(line[line.size() - 1])))
    return false;

  std::vector<std::string> rule =
      utils::string_split(utils::trim_whitespace(line), " ");
  std::size_t size = rule.size();
  PathPattern key(key_data);

  if (size < 2)
    return false;

  // Find or create routes for each method with this path pattern
  for (std::size_t i = 0; i < mets.size(); ++i) {
    std::size_t targetRouteIndex =
        routes.size(); // Will be set to existing route index or stay as size
                       // (indicating new route)

    // Find existing route with matching method and path (exact match for
    // updating properties)
    for (std::size_t j = 0; j < routes.size(); ++j) {
      // For updating route properties, we need exact path match, not wildcard
      // match
      if (routes[j].method == mets[i]) {
        // Compare path segments for exact match
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

    // If not found, create a new route
    if (targetRouteIndex == routes.size()) {
      RouteRule newRoute;
      newRoute.method = mets[i];
      newRoute.path = key;
      newRoute.op = UNDEFINED;
      newRoute.max_body_KB = 1;
      routes.push_back(newRoute);
      // targetRouteIndex is already set to the correct value (old size, which
      // is the new index)
    }

    // Update the route based on rule type (using index to avoid pointer
    // invalidation)
    if (rule[0] == "?") {
      std::string index = get_valid_index_file(rule[1]);
      if (size != 2 || index == "")
        return false;
      routes[targetRouteIndex].index = index;
    } else if (rule[0] == "@") {
      if (size != 2)
        return false;
      routes[targetRouteIndex].auth_info = rule[1];
    } else if (rule[0] == "->{}") {
      int max = parse_max_body_size(rule[1]);
      if (max == -1 || size != 2)
        return false;
      routes[targetRouteIndex].max_body_KB = max;
    } else if (rule[0] == "!") {
      std::string errPageLine = rule[1]; // Make a copy to avoid modification
      int err_key = parse_error_page_entry(errPageLine); // WebserverConfig::apply_default_err_page_entry 함수로 수정 해야함
      if (err_key == 0 || size != 2)
        return false;
      routes[targetRouteIndex].error_pages[err_key] = errPageLine; 
    } else
      return false;
  }

  return true;
}

RuleOperator ServerConfig::parse_rule_operator(const std::string &indicator) {
  if (indicator == "<-")
    return (SERVEFROM);
  else if (indicator == "->")
    return (POINT);
  else if (indicator == "<i-")
    return (AUTOINDEX);
  else if (indicator == "=300>")
    return (MULTIPLECHOICES);
  else if (indicator == "=301>")
    return (REDIRECT);
  else if (indicator == "=302>")
    return (FOUND);
  else if (indicator == "=303>")
    return (SEEOTHER);
  else if (indicator == "=304>")
    return (NOTMODIFIED);
  else if (indicator == "=307>")
    return (TEMPORARYREDIRECT);
  else if (indicator == "=308>")
    return (PERMANENTREDIRECT);
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
      err_meg = data[2] + "]: The route rule contains an undefined operator type. (route rule, the operator used in the rule is not registered in the supported operation set, so it cannot be interpreted within the routing logic).";
      return false;
    }
    route.index = "";
    route.auth_info = "";
    route.max_body_KB = 1;

    for (size_t j = 0; j < path_url.size(); ++j) {
      route.path = path_url[j];
      route.root = root_url;
      if (!has_compatible_wildcards(route.path, route.root)) {
        err_meg = route.path.to_string() + ", " + route.root.to_string() + "]: The route rule has a mismatch in wildcard usage around the operator. (route rule, the number of wildcard * occurrences in the left-hand and right-hand expressions must match, but the provided rule has inconsistent wildcard counts, making the mapping invalid).";
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
    if (err_meg != "")
      return false;
    else if (apply_route_rule_entry(mets, route_line_data[1], line)) { // 수정해야 함
      if (err_meg != "")
        return false;
      continue;
    }
  }
  err_meg = "";
  return true;
}

// Find a route that matches the given method and path
// 리다이렉션, 오토인덱스가 rule의 wildcard 상관없이 매칭이 가능
RouteRule const *ServerConfig::find_route(Request::Method method,
                                          const std::string &path) const {
  PathPattern pathPattern(path);

  // Iterate through all routes to find a match
  for (size_t i = 0; i < routes.size(); ++i) {
    if (routes[i].method == method && routes[i].path.matches(pathPattern)) {
      return &routes[i];
    }
  }
  return NULL;
}

RouteRule_CGI const *ServerConfig::find_route_cgi(Request::Method method,
                                          const std::string &path) const {
  PathPattern pathPattern(path);

  for (size_t i = 0; i < R_CGI.size(); ++i) {
    if (R_CGI[i].get_method() == method && R_CGI[i].get_path().matches(pathPattern)) {
      return &R_CGI[i];
    }
  }
  return NULL;
}

std::ostream &operator<<(std::ostream &os, const PathPattern &data) {

    os << data.to_string();
  return (os);
}

static std::string what_RuleOperator(RuleOperator op) {
  if (op == MULTIPLECHOICES)
    return ("MULTIPLECHOICES (=300>)");
  else if (op == REDIRECT)
    return ("REDIRECT (=301>)");
  else if (op == FOUND)
    return ("FOUND (=302>)");
  else if (op == SEEOTHER)
    return ("SEEOTHER (=303>)");
  else if (op == NOTMODIFIED)
    return ("NOTMODIFIED (=304>)");
  else if (op == TEMPORARYREDIRECT)
    return ("TEMPORARYREDIRECT (=307>)");
  else if (op == PERMANENTREDIRECT)
    return ("PERMANENTREDIRECT (=308>)");
  else if (op == AUTOINDEX)
    return ("AUTOINDEX (<i-)");
  else if (op == POINT)
    return ("POINT (->)");
  else
    return ("SERVEFROM (<-)");
}

std::ostream &operator<<(std::ostream &os, const ServerConfig &data) {
  os << "Server Response Time(s): " << data.get_server_response_time()
     << std::endl;

  const std::map<std::string, std::string> &header = data.get_header();
  std::map<std::string, std::string>::const_iterator header_it;
  os << "\n\n\n<<Header>>";
  for (header_it = header.begin(); header_it != header.end(); ++header_it) {
    os << "\n\tkey: " << header_it->first << ", value: " << header_it->second
       << std::endl;
  }

  os << "\n\n\n<<Server CGI>>";
  const CGI &s = data.get_serve_cgi();
  CGI::const_iterator s_it;
  if (s.empty())
    os << "\n\tEmpty" << std::endl;
  for (s_it = s.begin(); s_it != s.end(); ++s_it) {
    os << "\n\tkey: " << s_it->first << std::endl;
    if (s_it->second.empty())
      os << "\tvalue: nosniff" << std::endl;
    else {
      std::map<std::string, std::string>::const_iterator temp;
      for (temp = s_it->second.begin(); temp != s_it->second.end(); ++temp)
        os << "\tvalue: " << temp->first << " " << temp->second << std::endl;
    }
  }

  const std::vector<RouteRule> &routes = data.get_routes();
  os << "\n\n\n<<Routes>>";
  for (size_t i = 0; i < routes.size(); ++i) {
    const RouteRule &route = routes[i];
    os << "\n\nRoute: ";
    if (route.method == Request::GET)
      os << "GET";
    else if (route.method == Request::POST)
      os << "POST";
    else if (route.method == Request::DELETE)
      os << "DELETE";
    os << " " << route.path << std::endl;

    os << "\n\tRuleOperator: " << what_RuleOperator(route.op) << std::endl;
    os << "\tRedirect Target: " << route.redirect_target << std::endl;

    os << "\n\tRoot: " << route.root << std::endl;
    os << "\tIndex: " << route.index << std::endl;
    os << "\tAuth Info: " << route.auth_info << std::endl;
    os << "\tMax Body(KB): " << route.max_body_KB;
    if (route.error_pages.empty())
      os << "\n\tError Page: "
         << "empty map";
    else {
      std::map<int, std::string>::const_iterator err_it;
      for (err_it = route.error_pages.begin();
           err_it != route.error_pages.end(); ++err_it)
        os << "\n\tError Page: " << err_it->first << " " << err_it->second;
    }
  }

  std::vector<RouteRule_CGI> cgi = data.get_route_rule_cgi();
  os << "\n\n\n\n<<Route CGI>>\n";
  for (std::size_t i = 0; i < cgi.size(); ++i) {
    os << cgi[i];
  }
  if (cgi.size() == 0)
    os << "\n\tEmpty";
  os << "\n========================================================";
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
  // return route->path.rewrite_path(path, route->root);
}























