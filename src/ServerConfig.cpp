#include "webserv.h"

ServerConfig::ServerConfig() : err_line("Empty server block"), serverResponseTime(0) {}

ServerConfig::ServerConfig(std::ifstream &file) : err_line(""), serverResponseTime(3) {
  if (!set_ServerConfig(file)) {
    return ;
  }
  return ;
}

ServerConfig::~ServerConfig() {}

bool ServerConfig::set_ServerConfig(std::ifstream &file) {
  std::string line;

  while (std::getline(file, line)) {
    if (is_tab_or_space(line, 0))
      continue;
    else if (is_tab_or_space(line, 1)) {
      if (is_header(line)) {
        if (!parse_header_line(file, line)) {
          err_line = "Header syntax Error: " + err_line;
          return (false);
        }
      }
      else if (is_serverResponseTime(line))
        parse_serverResponseTime(line);
      else if (is_RouteRule(line)){
        if(!parse_RouteRule(line, file)) {// line을 같이 넘겨서 등록 추가
          err_line = "RouteRule syntax Error: " + err_line;
            return (false);
          }
      }
      else {
        err_line = "Invalid line Error: " + trim_space(line);
        return (false);
      }
    } 
    else
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

std::string ServerConfig::Geterr_line(void) { return (err_line); }

// header method
bool ServerConfig::is_header(const std::string &line) {
  std::string temp = trim_space(line);
  if (temp.empty())
    return (false);
  if (temp[0] == '[' && temp[1] == ']')
    return (true);
  return (false);
}

bool ServerConfig::parse_header_line(std::ifstream &file, std::string line) {
  std::string temp(line);
  std::vector<std::string> key_value = string_split(temp, ":");

  err_line = temp;
  if (key_value.size() != 2)
    return (false);
  std::string key = trim_space(key_value[0]);
  temp = key_value[1];
  if (!is_header_key(key) || !parse_header_value(temp, key))
    return (false);
  while (!temp.empty() && temp[temp.length() - 1] == ';' &&
         getline(file, temp)) {
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
  if (line[pos] != '(' || line.back() != ')')
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

static std::vector<std::string> get_pattern(std::string line)
{
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

static std::vector<std::vector<std::string>> make_paths_from_url_pattern \
(std::vector<std::vector<std::string>> paths, \
 std::vector<std::string> pattern, int index)
{
  std::vector<std::vector<std::string>> new_paths;
  std::string seg = paths[0][index];
  std::string prefix = "";
  std::string suffix = "";
  
  new_paths.reserve(paths.size() * pattern.size());
  size_t pos = seg.find('(');
  prefix = seg.substr(0, pos);
  pos = seg.find(')', pos + 1);
  if (pos != std::string::npos && pos + 1 < seg.size())
    suffix = seg.substr(pos + 1);

  for (size_t i = 0; i < paths.size(); ++i)
  {
    for (size_t j = 0; j < pattern.size(); ++j)
    {
      std::vector<std::string> new_path = paths[i];
      new_path[index] = prefix + pattern[j] + suffix;
      new_paths.push_back(new_path);
    }
  }
  return (new_paths);
}

static std::vector<std::vector<std::string>> expand_url_pattern(std::string line)
{
  std::vector<std::string> path(string_split(line, "/"));
  std::vector<std::vector<std::string>> paths;

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

bool ServerConfig::is_RouteRule(std::string line) { // 메서드가 맞는지 정도만 확인, path,root이 정상적으로 생겻는지 판단
  std::vector<std::string> split = string_split(line, " ");
  
  if (split.size() != 4 || parse_RuleOperator(split[2]) == UNDEFINE || is_url(split[1]) || is_url(split[3])) // 크기 확인, op확인
    return (false);

  std::vector<std::string> method = string_split(split[0], "|");
  for (size_t i = 0; i < method.size(); ++i) // 메서드 확인
  {
    if (method[i] == "GET")
      mets.push_back(GET);
    else if (method[i] == "POST")
      mets.push_back(POST);
    else if (method[i] == "DELETE")
      mets.push_back(DELETE);
    else
      return (false);
  }
  return (true);
}

bool ServerConfig::parse_Rule(std::vector<HttpMethod> mets, std::string key, std::string line) {
  (void)mets;
  (void)key;
  (void)line;
  // std::vector<std::string> rule = string_split(line, " ");

    // if (rule[0] == "?")
    //   for (size_t i = 0; i < mets.size(); ++i)
    //     routes[std::make_pair(mets[i], key)].index = rule[1];
    // else if (rule[0] == "@")
    //   for (size_t i = 0; i < mets.size(); ++i)
    //     routes[std::make_pair(mets[i], key)].authInfo = rule[1];
    // else if (rule[0] == "->{}")
    //   for (size_t i = 0; i < mets.size(); ++i)
    //     routes[std::make_pair(mets[i], key)].maxBodyMB = rule[1];
    // else if (rule[0] == "!")
    //   for (size_t i = 0; i < mets.size(); ++i)
    //     routes[std::make_pair(mets[i], key)].errorPages = rule[1];
    // else
    //   return (false);
  return (true);
}

RuleOperator ServerConfig::parse_RuleOperator(std::string indicator)
{
  if (indicator == "<-")
    return (SEREVEFROM);
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
    return (UNDEFINE);
}

bool ServerConfig::parse_Httpmethod(std::vector<std::string> data, std::vector<HttpMethod> mets) {
  RouteRule route;
  std::vector<std::vector<std::string> > path_url;
  std::vector<std::vector<std::string> > root_url;

  if (data.size() != 4)
    return (false);
  for (size_t i = 0; i < mets.size(); ++i)
  {
    route.method = mets[i];
    route.op = parse_RuleOperator(data[2]); // 타입 확인후 에러 처리 준비
    route.index = "";
    route.authInfo = "";
    route.maxBodyMB = 1;
    path_url = expand_url_pattern(data[1]);
    root_url = expand_url_pattern(data[3]);
    if (path_url.size() < 1 || root_url.size() < 1 || path_url.size() != root_url.size())
      return (false);
    for (size_t i = 0; i < path_url.size(); ++i) // path와 root이 정상적인지, 서로 패턴이 매칭이 되는지 확인
    {
      route.path = path_url[i];
      route.root = root_url[i];
      routes[std::make_pair(route.method, route.path)] = route;
    }
  }
  return (true);
}

bool ServerConfig::parse_RouteRule(std::string method_line, std::ifstream &file) {
  std::string line;
  std::vector<HttpMethod> mets;
  std::vector<std::string> method_line_data = string_split(method_line, " ");
  std::vector<std::string> method = string_split(method_line_data[0], "|");
  
  err_line = method_line;
  for (size_t i = 0; i < method.size(); ++i)
  {
    if (method[i] == "GET")
      mets.push_back(GET);
    else if (method[i] == "POST")
      mets.push_back(POST);
    else if (method[i] == "DELETE")
      mets.push_back(DELETE);
    else
      return (false);
  }

  if (parse_Httpmethod(method_line_data, mets))
    return (false);

  while (std::getline(file, line))
  {
    err_line = line;
    if (is_tab_or_space(line, 0))
      break;
    else if (is_tab_or_space(line, 2) != false)
      return (false);
    else if (parse_Rule(mets, method_line_data[1], line))
      continue ;
    else
      return (false);
  }
  err_line = "";
  return (true);
}

