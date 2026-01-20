#include "webserv.h"

ServerConfig::ServerConfig() : is_success(false) {}

ServerConfig::ServerConfig(std::ifstream &file) : serverResponseTime(3) {
  is_success = set_ServerConfig(file);
  (void)serverResponseTime;
  (void)routes;
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
        ;
      else
        err_line.push_back(trim_space(line));
    } else
      break;
  }
  for (size_t i = 0; i < err_line.size(); ++i) {
    std::cout << "err_line: " << err_line[i] << std::endl;
  }
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
    if (temp.size() == 1 && temp[0] == "\'nosniff\'")
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
      // std::cout << "key: [" << key << "] value_key: [" << temp[0] << "]
      // value: [" << header[key][temp[0]] << "]" << std::endl;
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
    if (data > 900)
      return (false);
  }
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
  (void)line;
  return (false);
}
