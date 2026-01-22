#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : is_success(false) {}

ServerConfig::ServerConfig(std::ifstream &file) : serverResponseTime(3) {
  is_success = set_ServerConfig(file);
}

ServerConfig::~ServerConfig() {}

bool ServerConfig::set_ServerConfig(std::ifstream &file) {
  std::string line;

  while (std::getline(file, line)) {
    if (is_tab_or_space(line, 0))
      continue;
    else if (is_tab_or_space(line, 1)) {
      if (is_header(line))
        parse_header_line(file, line);
      else if (is_serverResponseTime(line))
        parse_serverResponseTime(line);
      else if (is_RouteRule(line))
        parse_RouteRule(line, file); // line을 같이 넘겨서 등록 추가
      else
        err_line.push_back(trim_space(line));
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
  // for (size_t i = 0; i < err_line.size(); ++i) {
  //   std::cout << "err_line: " << err_line[i] << std::endl;
  // }
  return (true);
}

bool ServerConfig::getis_succes(void) { return (is_success); }

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

  if (key_value.size() != 2)
    return (false);

  std::string key = trim_space(key_value[0]);
  temp = key_value[1];
  if (!is_header_key(key) || !parse_header_value(temp, key))
    return (false);
  while (!temp.empty() && temp[temp.length() - 1] == ';' &&
         getline(file, temp)) {
    if (!is_tab_or_space(temp, 2))
      return (false);
    if (!parse_header_value(temp, key))
      return (false);
  }
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
    return (false);
  if (value[value.length() - 1] == ' ' || value[value.length() - 1] == '\t')
    return (false);
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
    } else {
      err_line.push_back(trim_space(value));
      return (false);
    }
  }
  return (true);
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
bool ServerConfig::is_RouteRule(std::string line) {
  std::vector<std::string> Route = string_split(line, " ");
  if (Route[0] == "GET")
    return (parse_GET(Route));
  else if(Route[0] == "POST")
    return (parse_POST(Route));
  else if (Route[0] == "DELETE")
    return (parse_DELETE(Route));
  else if (Route[0] == "GET|POST|DELETE")
    return (true);
  else
    return (false);
}

bool ServerConfig::parse_RouteRule(std::string method, std::ifstream &file) {
  std::string line;
  std::vector<std::string> met = string_split(string_split(method, " ")[0], "|");
  std::string key = string_split(method, " ")[1];

  while (std::getline(file, line))
  {
    if (is_tab_or_space(line, 0))
      break;
    else if (is_tab_or_space(line, 2) != false)
      return (false);
    else if (parse_Rule(met, key, line))
      continue ;
    else
      return (false);
  }
  return (true);
}

bool ServerConfig::parse_GET(std::vector<std::string> line) {
  RouteRule get;
  
  if (line.size() != 4)
    return (false);
  get.method = GET;
  get.path = string_split(line[1], "/");
  get.op = is_RuleOperator(line[2]);
  get.root = string_split(line[3], "/");
  get.index = "";
  get.authInfo = "";
  get.maxBodyMB = 1;
  return (true);
}

bool ServerConfig::parse_POST(std::vector<std::string> line) {
  RouteRule post;
  
  if (line.size() != 4)
    return (false);
  post.method = POST;
  post.path = string_split(line[1], "/");
  post.op = is_RuleOperator(line[2]);
  post.root = string_split(line[3], "/");
  post.index = "";
  post.authInfo = "";
  post.maxBodyMB = 1;
  return (true);
}

bool ServerConfig::parse_DELETE(std::vector<std::string> line) {
  RouteRule del;
  
  if (line.size() != 4)
    return (false);
  del.method = DELETE;
  del.path = string_split(line[1], "/");
  del.op = is_RuleOperator(line[2]);
  del.root = string_split(line[3], "/");
  del.index = "";
  del.authInfo = "";
  del.maxBodyMB = 1;
  return (true);
}

bool ServerConfig::parse_Rule(std::vector<std::string> met, std::string key, std::string line) {
  (void)met;
  (void)key;
  std::vector<std::string> rule = string_split(line, " ");
  if (rule[0] == "?")
    ;
  else if (rule[0] == "@")
    ;
  else if (rule[0] == "->{}")
    ;
  else if (rule[0] == "!")
    ;
  else
    return (false);
  return (true);
}

RuleOperator ServerConfig::is_RuleOperator(std::string indicator)
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
