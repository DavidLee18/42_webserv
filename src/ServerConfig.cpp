#include "webserv.h"

ServerConfig::ServerConfig(FileDescriptor &file) {
  err_line = "";
  server_response_time = 3;
  end_flag = 0;
  if (!set_ServerConfig(file)) {
    return;
  }
  return;
}

bool ServerConfig::set_ServerConfig(FileDescriptor &fd) {
  std::string line;

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "") {
      err_line = "FileDescriptor Error: " + temp.error();
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
    if (utils::match_indent_level(line, 1)) {
      line = utils::trim_whitespace(line);
      if (is_header(line)) {
        if (!parse_header_line(fd, line)) {
          err_line = "Header syntax Error: " + err_line;
          return false;
        }
      } else if (RouteRule_CGI::matches_cgi_syntax(line)) {
        std::string key;
        std::map<std::string, std::string> temp;
        err_line = RouteRule_CGI::parse_executable(line, key, temp);
        if (err_line != "")
          return false;
        if (S_CGI.find(key) != S_CGI.end()) {
          err_line = "Error: \"" + line + "\" duplicate key error";
          return false;
        }
        S_CGI[key] = temp;
      } else if (is_server_response_time(line))
        parse_server_response_time(line);
      else if (is_RouteRule(line)) {
        if (!parse_RouteRule(line, fd)) {
          err_line = "RouteRule syntax Error: " + err_line;
          return false;
        }
      } else if (RouteRule_CGI::is_valid_cgi_config(line)) {
        RouteRule_CGI temp(fd, line);
        if (temp.get_err() != "") {
          err_line = temp.get_err();
          return false;
        }
        R_CGI.push_back(temp);
      } else {
        err_line = "Invalid line Error: " + utils::trim_whitespace(line);
        return false;
      }
    } else
      return false;
  }
  return true;
}

// header method
bool ServerConfig::is_header(const std::string &line) {
  std::string temp = utils::trim_whitespace(line);
  if (temp.empty())
    return (false);
  if (temp[0] == '[' && temp[1] == ']')
    return (true);
  return (false);
}

bool ServerConfig::parse_header_line(FileDescriptor &fd, std::string line) {
  std::string temp(line);
  std::vector<std::string> key_value = utils::string_split(temp, ":");

  err_line = temp;
  if (key_value.size() != 2)
    return (false);
  std::string key = utils::trim_whitespace(key_value[0]);
  temp = key_value[1];
  if (!is_header_key(key) || !parse_header_value(temp, key))
    return (false);
  while (!temp.empty() && temp[temp.length() - 1] == ';') {
    Result<std::string> fd_line = fd.read_file_line();
    if (fd_line.error() != "") {
      err_line = "FileDescriptor Error: " + fd_line.error();
      return (false);
    }
    if (fd_line.value() == "\n" || fd_line.value() == "") {
      end_flag += 1;
      break;
    }
    temp = line = utils::remove_char(fd_line.value(), '\n');
    err_line = temp;
    if (!utils::match_indent_level(temp, 2))
      return (false);
    if (!parse_header_value(temp, key))
      return (false);
  }
  err_line = "";
  return (true);
}

bool ServerConfig::is_header_key(std::string &key) {
  std::vector<std::string> temp;

  if (key.empty())
    return (false);
  temp = utils::string_split(key, " ");
  if (temp.size() == 3)
    key = temp[2];
  return (true);
}

bool ServerConfig::parse_header_value(std::string value,
                                      const std::string key) {
  if (value.empty())
    return false;
  if (value[value.length() - 1] == ' ' || value[value.length() - 1] == '\t')
    return false;
  std::vector<std::string> values =
      utils::string_split(utils::trim_whitespace(value), ";");
  std::vector<std::string> temp;
  for (size_t i = 0; i < values.size(); i++) {
    values[i] = utils::trim_whitespace(values[i]);
    temp = utils::string_split(values[i], " ");
    if (temp.size() == 1 && temp[0] == "\"nosniff\"")
      header[key];
    else if (temp.size() == 2) {
      if (temp[1].size() < 2)
        return false;
      if (temp[1][0] != '\'' || temp[1][temp[1].size() - 1] != '\'')
        return false;
      for (size_t j = 1; j + 1 < temp[1].size(); ++j)
        if (temp[1][j] == '\'')
          return false;
      header[key][temp[0]] = temp[1];
    } else
      return false;
  }
  return true;
}

// server_response_time method
bool ServerConfig::is_server_response_time(std::string &line) {
  if (line.empty() || line[line.length() - 1] == ' ' ||
      line[line.length() - 1] == '\t')
    return (false);
  line = utils::trim_whitespace(line);
  if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
    return (false);
  int data = 0;
  for (size_t i = 3; i < line.length(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i])))
      return (false);
    data = data * 10 + (line[i] - '0');
  }
  if (data > 900 || 0 >= data)
    return (false);
  return (true);
}

void ServerConfig::parse_server_response_time(std::string line) {
  line.erase(0, 3);
  std::stringstream oss;
  oss << line;
  oss >> server_response_time;
}

// RouteRule method

static bool is_pattern(std::string line) {
  size_t pos = line.find("*.");
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

  size_t count = 0;
  size_t start = 0;
  while (1) {
    size_t pipePos = inside.find('|', start);
    std::string ext = inside.substr(start, pipePos - start);
    if (ext.empty())
      return false;
    for (size_t i = 0; i < ext.size(); ++i) {
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

static std::vector<std::string> get_pattern(std::string line) {
  std::vector<std::string> temp = utils::string_split(line, "*");
  std::string pattern = temp[temp.size() - 1];
  size_t l = pattern.find('(');
  size_t r = pattern.find(')', l == std::string::npos ? 0 : l + 1);

  if (l != std::string::npos && r != std::string::npos && r > l)
    pattern = pattern.substr(l + 1, r - l - 1);
  temp = utils::string_split(pattern, "|");
  // for (size_t i = 0; i < temp.size(); ++i)
  // {
  //   std::cout << temp[i] << std::endl;
  // }
  return (temp);
}

static std::vector<std::vector<std::string> >
make_paths_from_url_pattern(std::vector<std::vector<std::string> > paths,
                            std::vector<std::string> pattern, size_t index) {
  std::vector<std::vector<std::string> > new_paths;
  std::string seg = paths[0][index];
  std::string prefix = "";
  std::string suffix = "";

  new_paths.reserve(paths.size() * pattern.size());
  size_t pos = seg.find('(');
  prefix = seg.substr(0, pos);
  pos = seg.find(')', pos + 1);
  if (pos != std::string::npos && pos + 1 < seg.size())
    suffix = seg.substr(pos + 1);

  for (size_t i = 0; i < paths.size(); ++i) {
    for (size_t j = 0; j < pattern.size(); ++j) {
      std::vector<std::string> new_path = paths[i];
      new_path[index] = prefix + pattern[j] + suffix;
      new_paths.push_back(new_path);
    }
  }
  return (new_paths);
}

static std::vector<std::vector<std::string> >
expand_url_pattern(std::string line) {
  std::vector<std::string> path(utils::string_split(line, "/"));
  std::vector<std::vector<std::string> > paths;

  paths.push_back(path);
  for (size_t i = 0; i < path.size(); ++i) {
    if (is_pattern(path[i])) {
      std::vector<std::string> pattern = get_pattern(path[i]);
      paths = make_paths_from_url_pattern(paths, pattern, i);
    } else
      continue;
  }
  return (paths);
}

static bool is_url(std::string url) {
  size_t i = 0;
  int flag = 0;

  for (; i < url.size(); ++i) {
    if (url[i] == '*')
      flag += 1;
    else if (url[i] == '/')
      flag = 0;
    if (flag > 1)
      return (false);
  }
  return (true);
}

bool ServerConfig::is_RouteRule(std::string line) {
  if (line.empty())
    return (false);
  if (std::isspace(static_cast<unsigned char>(line[line.size() - 1])))
    return (false);
  std::vector<std::string> split = utils::string_split(line, " ");
  if (split.size() != 4 || parse_RuleOperator(split[2]) == UNDEFINED ||
      !is_url(split[1]) || !is_url(split[3])) // 크기 확인, op확인
    return (false);

  std::vector<std::string> method = utils::string_split(split[0], "|");
  for (size_t i = 0; i < method.size(); ++i) {
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

static int max_body_KB_parse(std::string line) {
  size_t i = 0;
  int maxbody = 0;
  for (; i < line.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i])))
      break;
    maxbody = maxbody * 10 + (line[i] - '0');
  }
  if (i <= 0)
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

static std::string index_parse(std::string line) {
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

static int errPage_parse(std::string &line) {
  int key = 0;
  std::vector<std::string> key_and_value = utils::string_split(line, ":");

  if (key_and_value.size() != 2)
    return (0);
  for (size_t i = 0; i < key_and_value[0].size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(key_and_value[0][i])))
      return (0);
    key = key * 10 + (key_and_value[0][i] - '0');
  }
  line = key_and_value[1];
  return (key);
}

bool ServerConfig::parse_rule(std::vector<Request::Method> mets,
                              std::string key_data, std::string line) {
  if (line.empty())
    return (false);
  if (std::isspace(static_cast<unsigned char>(line[line.size() - 1])))
    return (false);

  std::vector<std::string> rule =
      utils::string_split(utils::trim_whitespace(line), " ");
  size_t size = rule.size();
  PathPattern key(key_data);

  if (size < 2)
    return (false);

  // Find or create routes for each method with this path pattern
  for (size_t i = 0; i < mets.size(); ++i) {
    size_t targetRouteIndex =
        routes.size(); // Will be set to existing route index or stay as size
                       // (indicating new route)

    // Find existing route with matching method and path (exact match for
    // updating properties)
    for (size_t j = 0; j < routes.size(); ++j) {
      // For updating route properties, we need exact path match, not wildcard
      // match
      if (routes[j].method == mets[i]) {
        // Compare path segments for exact match
        const std::vector<std::string> &routePath = routes[j].path.get_path();
        const std::vector<std::string> &keyPath = key.get_path();
        if (routePath.size() == keyPath.size()) {
          bool exactMatch = true;
          for (size_t k = 0; k < routePath.size(); ++k) {
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
      std::string index = index_parse(rule[1]);
      if (size != 2 || index == "")
        return (false);
      routes[targetRouteIndex].index = index;
    } else if (rule[0] == "@") {
      if (size != 2)
        return (false);
      routes[targetRouteIndex].auth_info = rule[1];
    } else if (rule[0] == "->{}") {
      int max = max_body_KB_parse(rule[1]);
      if (max == -1 || size != 2)
        return (false);
      routes[targetRouteIndex].max_body_KB = max;
    } else if (rule[0] == "!") {
      std::string errPageLine = rule[1]; // Make a copy to avoid modification
      int err_key = errPage_parse(errPageLine);
      if (err_key == 0 || size != 2)
        return (false);
      routes[targetRouteIndex].error_pages[err_key] = errPageLine;
    } else
      return (false);
  }

  return (true);
}

RuleOperator ServerConfig::parse_RuleOperator(std::string indicator) {
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

bool ServerConfig::is_matching(PathPattern path, PathPattern root) {
  std::vector<std::string> path_pattern = path.get_path();
  std::vector<std::string> root_pattern = root.get_path();

  int path_wild = 0;
  int root_wild = 0;
  size_t j = 0;
  for (size_t i = 0; i < path_pattern.size(); ++i) {
    if (std::string::npos != path_pattern[i].find('*')) {
      path_wild++;
      for (; j < root_pattern.size(); ++j) {
        if (path_pattern[i] == root_pattern[j] ||
            (path_pattern[i] == "*" &&
             std::string::npos != root_pattern[j].find('*'))) {
          j++;
          root_wild++;
          break;
        }
      }
    }
  }
  if (path_wild != root_wild)
    return false;
  return true;
}

bool ServerConfig::parse_Httpmethod(std::vector<std::string> data,
                                    std::vector<Request::Method> mets) {
  RouteRule route;
  std::vector<std::vector<std::string> > path_url;
  std::vector<std::vector<std::string> > root_url;

  if (data.size() != 4)
    return (false);
  for (size_t i = 0; i < mets.size(); ++i) {
    route.method = mets[i];
    route.op = parse_RuleOperator(data[2]);
    if (route.op == UNDEFINED)
      return (false);
    route.index = "";
    route.auth_info = "";
    route.max_body_KB = 1;
    path_url = expand_url_pattern(data[1]);
    root_url = expand_url_pattern(data[3]);
    if (path_url.size() < 1 || root_url.size() < 1 ||
        path_url.size() != root_url.size())
      return (false);

    for (size_t j = 0; j < path_url.size(); ++j) {
      route.path = path_url[j];
      route.root = root_url[j];
      if (!is_matching(route.path, route.root))
        return (false);
      if (route.op == REDIRECT)
        route.redirect_target = root_url[j];
      if (route.op == AUTOINDEX) {
        route.path.add_path("*");
        route.root.add_path("*");
      }
      routes.push_back(route);
    }
  }
  return (true);
}

bool ServerConfig::parse_RouteRule(std::string method_line,
                                   FileDescriptor &fd) {
  std::string line;
  std::vector<Request::Method> mets;
  std::vector<std::string> method_line_data =
      utils::string_split(method_line, " ");
  std::vector<std::string> method =
      utils::string_split(method_line_data[0], "|");

  err_line = method_line;
  for (size_t i = 0; i < method.size(); ++i) {
    if (method[i] == "GET")
      mets.push_back(Request::GET);
    else if (method[i] == "POST")
      mets.push_back(Request::POST);
    else if (method[i] == "DELETE")
      mets.push_back(Request::DELETE);
    else
      return (false);
  }

  if (!parse_Httpmethod(method_line_data, mets))
    return (false);

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "") {
      err_line = "FileDescriptor Error: " + temp.error();
      return (false);
    }
    if (temp.value() == "\n" || temp.value() == "") {
      end_flag += 1;
      break;
    }
    line = utils::remove_char(temp.value(), '\n');
    err_line = line;
    if (utils::match_indent_level(line, 2) == false)
      return (false);
    else if (parse_rule(mets, method_line_data[1], line))
      continue;
    else
      return (false);
  }
  err_line = "";
  return (true);
}

// Find a route that matches the given method and path
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

std::ostream &operator<<(std::ostream &os, const PathPattern &data) {
  const std::vector<std::string> &path = data.get_path();
  size_t max = path.size();
  for (size_t i = 0; i < max; ++i)
    os << "/" << path[i];
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

  const Header &header = data.get_header();
  Header::const_iterator header_it;
  os << "\n\n\n<<Header>>";
  for (header_it = header.begin(); header_it != header.end(); ++header_it) {
    os << "\n\tkey: " << header_it->first << std::endl;
    if (header_it->second.empty())
      os << "\tvalue: nosniff" << std::endl;
    else {
      std::map<std::string, std::string>::const_iterator temp;
      for (temp = header_it->second.begin(); temp != header_it->second.end();
           ++temp)
        os << "\tvalue: " << temp->first << " " << temp->second << std::endl;
    }
  }

  os << "\n\n\n<<Server CGI>>";
  const Server_CGI &s = data.get_serve_cgi();
  Server_CGI::const_iterator s_it;
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

std::string ServerConfig::rewrite_to(std::string from, PathPattern path,
                                     PathPattern to) const {
  std::vector<std::string> new_path = path.get_path();
  std::vector<std::string> wilds;
  std::vector<std::string> new_to = to.get_path();
  std::vector<std::string> split_from = utils::string_split(from, "/");

  for (std::size_t i = 0; i < new_path.size(); ++i) {
    if (i < split_from.size() && std::string::npos != new_path[i].find("*"))
      wilds.push_back(split_from[i]);
    if (i + 1 == new_path.size()) {
      if (std::string::npos != new_path[i].find("*"))
        i++;
      for (std::size_t j = i; j < split_from.size(); ++j)
        wilds.push_back(split_from[j]);
    }
  }

  std::size_t j = 0;
  for (std::size_t i = 0; i < new_to.size(); ++i) {
    if (j < wilds.size() && std::string::npos != new_to[i].find("*"))
      new_to[i] = wilds[j++];
    if (i + 1 == new_to.size()) {
      for (; j < wilds.size(); ++j)
        new_to.push_back(wilds[j]);
      break;
    }
  }

  if (new_to.empty())
    return "";

  std::string result = "/";
  result += new_to[0];
  for (std::size_t i = 1; i < new_to.size(); ++i) {
    if (new_to[i].find("*") != std::string::npos)
      continue;
    result += '/';
    result += new_to[i]; // 수정됨: result_to -> new_to
  }
  return (result);
}

std::string ServerConfig::get_to(Request::Method method,
                                 const std::string &path) const {
  const RouteRule *temp = find_route(method, path);
  if (!temp)
    return "";
  return rewrite_to(path, temp->path, temp->root);
}
