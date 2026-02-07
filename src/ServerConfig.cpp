#include "webserv.h"

bool PathPattern::match(std::string wildcard, std::string path) const {
  std::vector<std::string> data = string_split(wildcard, "*");
  if (2 < data.size())
    return (false);
  for (size_t i = 0; i < data.size(); ++i) {
    size_t pos = std::string::npos;
    if (path.find(data[i]) == std::string::npos)
      return (false);
    else if (pos == std::string::npos + 1)
      return (false);
  }
  return (true);
}

bool PathPattern::operator==(const std::string &line) const {
  return (!(*this < line) && !(line < *this));
}

bool PathPattern::operator<(const PathPattern &other) const {
  size_t l = MIN(path.size(), other.path.size());
  for (size_t i = 0; i < l; i++) {
    std::vector<std::string> ps1 = string_split(path[i], "*"),
                             ps2 = string_split(other.path[i], "*");
    size_t l_ = MIN(ps1.size(), ps2.size());
    for (size_t j = 0; j < l_; j++) {
      if (ps1[j] != "*" && ps2[j] != "*" && ps1[j] != ps2[j]) {
        return ps1[j] < ps2[j];
      }
    }
    if (ps1.size() != ps2.size())
      return ps1.size() < ps2.size();
  }
  return path.size() < other.path.size();
}

bool PathPattern::operator<(std::string const &other) const {
  return (*this < PathPattern(string_split(other, "/")));
}

bool operator<(std::string const &l, PathPattern const &r) {
  return (!(r < l) && !(l == r));
}

ServerConfig::ServerConfig(FileDescriptor &file) {
  err_line = "";
  serverResponseTime = 3;
  if (!set_ServerConfig(file)) {
    return ;
  }
  return ;
}

bool ServerConfig::set_ServerConfig(FileDescriptor &fd) {
  std::string line;

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "")
      return (false);
    else if (temp.value() == "")
      break;
    else if (temp.value() == "\n")
      continue ;
    line = trim_char(temp.value(), '\n');
    if (is_tab_or_space(line, 1)) {
      if (is_header(line)) {
        if (!parse_header_line(fd, line)) {
          err_line = "Header syntax Error: " + err_line;
          return (false);
        }
      } else if (is_serverResponseTime(line))
        parse_serverResponseTime(line);
      else if (is_RouteRule(line)) {
        if (!parse_RouteRule(line, fd)) {
          err_line = "RouteRule syntax Error: " + err_line;
          return (false);
        }
      } else {
        err_line = "Invalid line Error: " + trim_space(line);
        return (false);
      }
    } else
      break;
  }
  // for (size_t i = 0; i < routes.size(); ++i)
  // {
  //   std::cout << "routes: " << routes[i].method << std::endl;
  //   std::cout << "path: ";
  //   for (size_t j = 0; j < routes[i].path.size(); ++j)
  //   {
  //     std::cout << routes[i].path[j] << " ";
  //   }
  //   std::cout << std::endl;
  // }
  return (true);
}

// header method
bool ServerConfig::is_header(const std::string &line) {
  std::string temp = trim_space(line);
  if (temp.empty())
    return (false);
  if (temp[0] == '[' && temp[1] == ']')
    return (true);
  return (false);
}

bool ServerConfig::parse_header_line(FileDescriptor &fd, std::string line) {
  std::string temp(line);
  std::vector<std::string> key_value = string_split(temp, ":");

  err_line = temp;
  if (key_value.size() != 2)
    return (false);
  std::string key = trim_space(key_value[0]);
  temp = key_value[1];
  if (!is_header_key(key) || !parse_header_value(temp, key))
    return (false);
  while (!temp.empty() && temp[temp.length() - 1] == ';') {
    Result<std::string> fd_line = fd.read_file_line();
    if (fd_line.error() != "")
      return (false);
    if (fd_line.value() == "\n" || fd_line.value() == "")
      break ;
    temp = line = trim_char(fd_line.value(), '\n');
    err_line = temp;
    if (!is_tab_or_space(temp, 2))
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
  temp = string_split(key, " ");
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
  std::vector<std::string> values = string_split(trim_space(value), ";");
  std::vector<std::string> temp;
  for (size_t i = 0; i < values.size(); i++) {
    values[i] = trim_space(values[i]);
    temp = string_split(values[i], " ");
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
    }
    else
      return false;
  }
  return true;
}

// serverResponseTime method
bool ServerConfig::is_serverResponseTime(std::string &line) {
  if (line.empty() || line[line.length() - 1] == ' ' ||
      line[line.length() - 1] == '\t')
    return (false);
  line = trim_space(line);
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

void ServerConfig::parse_serverResponseTime(std::string line) {
  line.erase(0, 3);
  std::stringstream oss;
  oss << line;
  oss >> serverResponseTime;
}

// RouteRule method

static bool is_pattern(std::string line)
{
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
  while (1)
  {
      size_t pos = inside.find('|', start);
      std::string ext = inside.substr(start, pos - start);
      if (ext.empty())
          return false;
      for (size_t i = 0; i < ext.size(); ++i)
      {
          if (!std::isalnum(static_cast<unsigned char>(ext[i])))
              return false;
      }
      count++;
      if (pos == std::string::npos)
          break;
      start = pos + 1;
  }
  return count >= 2;
}

static std::vector<std::string> get_pattern(std::string line) {
  std::vector<std::string> temp = string_split(line, "*");
  std::string pattern = temp[temp.size() - 1];
  size_t l = pattern.find('(');
  size_t r = pattern.find(')', l == std::string::npos ? 0 : l + 1);

  if (l != std::string::npos && r != std::string::npos && r > l)
    pattern = pattern.substr(l + 1, r - l - 1);
  temp = string_split(pattern, "|");
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
  std::vector<std::string> path(string_split(line, "/"));
  std::vector<std::vector<std::string> > paths;

  paths.push_back(path);
  for (size_t i = 0; i < path.size(); ++i)
  {
    if (is_pattern(path[i]))
    {
      std::vector<std::string> pattern = get_pattern(path[i]);
      paths = make_paths_from_url_pattern(paths, pattern, i);
    }
    else
      continue;
  }
  return (paths);
}

static bool is_url(std::string url)
{
  size_t i = 0;
  int flag = 0;

  for (; i < url.size(); ++i)
  {
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
  if (std::isspace(line[line.size() - 1]))
    return (false);
  std::vector<std::string> split = string_split(line, " ");
  if (split.size() != 4 || parse_RuleOperator(split[2]) == UNDEFINED || !is_url(split[1]) || !is_url(split[3])) // 크기 확인, op확인
    return (false);

  std::vector<std::string> method = string_split(split[0], "|");
  for (size_t i = 0; i < method.size(); ++i)
  {
    if (method[i] == "GET")
      continue ;
    else if (method[i] == "POST")
      continue ;
    else if (method[i] == "DELETE")
      continue ;
    else
      return (false);
  }
  return (true);
}

static int maxBodyKB_parse(std::string line)
{
  size_t i = 0;
  int maxbody = 0;
  for (; i < line.size(); ++i)
  {
    if (!std::isdigit(line[i]))
      break;
    maxbody = maxbody * 10 + (line[i] - '0');
  }
  line = line.substr(i);
  if (line.empty() || line == "KB" || line == "KiB")
    return (maxbody);
  else if (line == "MB")
    return (maxbody * 1000);
  else if (line == "MiB")
    return (maxbody * 1024);
  return (-1);
}

static std::string index_parse(std::string line)
{
  size_t i = 0;
  for (; i < line.size(); ++i)
  {
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

static int errPage_parse(std::string& line)
{
  int key = 0;
  std::vector<std::string> key_and_value = string_split(line, ":");

  if (key_and_value.size() != 2)
    return (0);
  for (size_t i = 0; i < key_and_value[0].size(); ++i)
  {
    if (!std::isdigit(key_and_value[0][i]))
      return (0);
    key = key * 10 + (key_and_value[0][i] - '0');
  }
  line = key_and_value[1];
  return(key);
}

bool ServerConfig::parse_Rule(std::vector<Http::Method> mets, std::string key, std::string line)
{
  if (std::isspace(line[line.size() - 1]))
    return (false);

  std::vector<std::string> rule = string_split(trim_space(line), " ");
  size_t size = rule.size();

  if (size < 2)
    return (false);
  if (rule[0] == "?") {
    std::string index = index_parse(rule[1]);
    if (size != 2 || index == "")
      return (false);
    for (size_t i = 0; i < mets.size(); ++i)
      routes[std::make_pair(mets[i], key)].index = index;
  }
  else if (rule[0] == "@") {
    if (size != 2)
      return (false);
    for (size_t i = 0; i < mets.size(); ++i)
      routes[std::make_pair(mets[i], key)].authInfo = rule[1];
  }
  else if (rule[0] == "->{}") {
    int max = maxBodyKB_parse(rule[1]);
    if (max == -1 || size != 2)
      return (false);
    for (size_t i = 0; i < mets.size(); ++i)
      routes[std::make_pair(mets[i], key)].maxBodyKB = max;
  }
  else if (rule[0] == "!") {
    int err_key = errPage_parse(rule[1]);
    if (err_key == 0 || size != 2)
      return (false);
    for (size_t i = 0; i < mets.size(); ++i)
      routes[std::make_pair(mets[i], key)].errorPages[err_key] = rule[1];
  }
  else
    return (false);
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

bool ServerConfig::parse_Httpmethod(std::vector<std::string> data,
                                    std::vector<Http::Method> mets) {
  RouteRule route;
  std::vector<std::vector<std::string> > path_url;
  std::vector<std::vector<std::string> > root_url;

  if (data.size() != 4)
    return (false);
  for (size_t i = 0; i < mets.size(); ++i) {
    route.status_code = 200;
    route.method = mets[i];
    route.op = parse_RuleOperator(data[2]);
    if (route.op == UNDEFINED)
      return (false);
    route.index = "";
    route.authInfo = "";
    route.maxBodyKB = 1;
    path_url = expand_url_pattern(data[1]);
    root_url = expand_url_pattern(data[3]);
    if (path_url.size() < 1 || root_url.size() < 1 || path_url.size() != root_url.size())
      return (false);
    for (size_t i = 0; i < path_url.size(); ++i)
    {
      route.path = path_url[i];
      route.root = root_url[i];
      // if () //path 와 root의 규칙이 맞는지 확인하는 부분 추가
      if (route.op == REDIRECT)
        route.redirectTarget = root_url[i];
      routes[std::make_pair(route.method, route.path)] = route;
    }
  }
  return (true);
}

bool ServerConfig::parse_RouteRule(std::string method_line,
                                   FileDescriptor &fd) {
  std::string line;
  std::vector<Http::Method> mets;
  std::vector<std::string> method_line_data = string_split(method_line, " ");
  std::vector<std::string> method = string_split(method_line_data[0], "|");

  err_line = method_line;
  for (size_t i = 0; i < method.size(); ++i) {
    if (method[i] == "GET")
      mets.push_back(Http::GET);
    else if (method[i] == "POST")
      mets.push_back(Http::POST);
    else if (method[i] == "DELETE")
      mets.push_back(Http::DELETE);
    else
      return (false);
  }

  if (!parse_Httpmethod(method_line_data, mets))
    return (false);

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "")
      return (false);
    if (temp.value() == "\n" || temp.value() == "")
      break;
    line = trim_char(temp.value(), '\n');
    err_line = line;
    if (is_tab_or_space(line, 2) == false)
      return (false);
    else if (parse_Rule(mets, method_line_data[1], line))
      continue;
    else
      return (false);
  }
  err_line = "";
  return (true);
}

std::ostream& operator<<(std::ostream& os, const PathPattern& data)
{
  std::vector<std::string> path = data.Get_path();
  size_t max = path.size();
  for (size_t i = 0; i < max; ++i)
  {
    os << path[i];
    if (i + 1 < max)
      os << "/";
  }
  return (os);
}

static std::string what_RuleOperator(RuleOperator op)
{
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

std::ostream& operator<<(std::ostream& os, const ServerConfig& data)
{
  os << "Server Response Time(s): " << data.Get_ServerResponseTime() << std::endl;

  Header header = data.Get_Header();
  Header::iterator header_it;
  os << "\nHeader";
  for (header_it = header.begin(); header_it != header.end(); ++header_it)
  {
    os << "\n\tkey: " << header_it->first << std::endl;
    if (header_it->second.empty())
      os << "\tvalue: nosniff" << std::endl;
    else {
      std::map<std::string, std::string>::iterator temp;
      for (temp = header_it->second.begin(); temp != header_it->second.end(); ++temp)
          os << "\tvalue: " << temp->first << " " << temp->second << std::endl;
    }
  }

  std::map<std::pair<Http::Method, PathPattern>, RouteRule> routes = data.Get_Routes();
  std::map<std::pair<Http::Method, PathPattern>, RouteRule>::iterator routes_it;
  os << "\nRoutes";
  for (routes_it = routes.begin(); routes_it != routes.end(); ++routes_it)
  {
    const std::pair<Http::Method, PathPattern>& key = routes_it->first;
    const RouteRule value = routes_it->second;
    os << "\n\nRoute key: ";
    if (key.first == Http::GET)
      os << "GET, ";
    else if (key.first == Http::POST)
      os << "POST, ";
    else if (key.first == Http::DELETE)
      os << "DELETE, ";
    os << key.second << std::endl;
    os << "\tMethod: ";
    if (key.first == Http::GET)
      os << "GET";
    else if (key.first == Http::POST)
      os << "POST";
    else if (key.first == Http::DELETE)
      os << "DELETE";
    os << "\n\tPath: " << value.path << std::endl;
    os << "\tStatus code: " << value.status_code << std::endl;

    os << "\n\tRuleOperator: " << what_RuleOperator(value.op) << std::endl;
    os << "\tRedirect Target: " << value.redirectTarget << std::endl;

    os << "\n\tRoot: " << value.root << std::endl;
    os << "\tIndex: " << value.index << std::endl;
    os << "\tAuto Info: " << value.authInfo << std::endl;
    os << "\tMax Body(KB): " << value.maxBodyKB;
    if (value.errorPages.empty())
      os << "\n\tError Page: " << "empty map";
    else {
      std::map<int, std::string>::const_iterator err_it;
      for (err_it = value.errorPages.begin(); err_it != value.errorPages.end(); ++err_it)
        os << "\n\tError Page: " << err_it->first << " "  << err_it->second;
    }
  }
  os << "\n========================================================";
  return (os);
}
