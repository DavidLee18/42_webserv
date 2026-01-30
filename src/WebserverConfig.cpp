#include "WebserverConfig.hpp"

WebserverConfig::WebserverConfig(std::ifstream &file) {
  err_meg = "";
  if (!this->file_parsing(file)) {
    file.close();
    return;
  }

  file.close();
  return;
}

bool WebserverConfig::file_parsing(std::ifstream &file) {
  std::string line;

  while (std::getline(file, line) && is_tab_or_space(line, 0)) {
    if (line == "types =" || line == "types=") {
      if (!set_type_map(file))
        return (false);
    }
    else if (is_ServerConfig(line)) {
      if (!set_ServerConfig_map(file, line))
        return (false);
    }
    else
    {
      err_meg = "Invalid line Error: " + line;
      return (false);
    }
  }
  if (this->type_map.empty() || this->default_mime.length() == 0)
    return (false);
  return (true);
}
// for (std::map<std::string, std::string>::iterator it =
// this->type_map.begin();
//      it != this->type_map.end();
//      ++it)
// {
//     std::cout << it->first << " = " << it->second << std::endl;
// }
// std::cout << default_mime << std::endl;

WebserverConfig::~WebserverConfig() {};

// type_map method
std::vector<std::string> WebserverConfig::is_typeKey(const std::string &key) {
  int number_of_key = 0;
  std::string temp = trim_space(key);
  std::vector<std::string> key_data;

  if (temp.empty() || is_have_space(temp) || is_have_special(temp, "_|"))
    return (key_data);
  key_data = string_split(temp, "|");
  number_of_key = number_of_delim(temp, "|") + 1;
  if (key_data.size() != static_cast<std::size_t>(number_of_key))
    return (std::vector<std::string>());
  for (std::size_t i = 0; i < key_data.size(); ++i) {
    if (type_map.find(key_data[i]) != type_map.end())
      return (std::vector<std::string>());
  }
  return (key_data);
}

bool WebserverConfig::is_typeValue(const std::string &value) {
  std::vector<std::string> value_data;

  if (value.empty())
    return (false);
  char last = value[value.length() - 1];
  if (!std::isalnum(static_cast<unsigned char>(last)))
    return (false);
  std::string temp = trim_space(value);
  if (temp.empty() || is_have_space(temp) || is_have_special(temp, "/-"))
    return (false);
  for (std::size_t i = 1; i < temp.size(); ++i) {
    if (temp[i] == '-' && temp[i - 1] == '-')
      return (false);
  }
  value_data = string_split(temp, "/");
  if (value_data.size() != 2 || number_of_delim(temp, "/") != 1)
    return (false);
  std::string type = value_data[0];
  std::string subtype = value_data[1];
  if (type.empty() || subtype.empty())
    return (false);
  for (std::size_t i = 0; i < temp.size(); ++i) {
    if (temp[i] == '-') {
      if (i == 0 || i == temp.size() - 1)
        return (false);
      if (!std::isalnum(static_cast<unsigned char>(temp[i - 1])) ||
          !std::isalnum(static_cast<unsigned char>(temp[i + 1])))
        return (false);
    }
  }
  return (true);
}

bool WebserverConfig::parse_type_line(const std::string &line,
                                      std::vector<std::string> &keys_out,
                                      std::string &value_out) {
  if (number_of_delim(line, "->") != 1)
    return (false);
  std::vector<std::string> type_data = string_split(line, "->");
  if (type_data.size() != 2)
    return (false);
  std::vector<std::string> keys = is_typeKey(type_data[0]);
  if (keys.empty() || !is_typeValue(type_data[1]))
    return (false);
  keys_out = keys;
  value_out = trim_space(type_data[1]);
  return (true);
}

bool WebserverConfig::set_type_map(std::ifstream &file) {
  std::string line;
  std::string value;
  std::vector<std::string> keys;
  std::vector<std::string> map_data;

  while (std::getline(file, line) && is_tab_or_space(line, 1)) {
    if (!parse_type_line(line, keys, value)) {
      err_meg = "Type syntax Error: " + line;
      return (false);
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
      const std::string &k = keys[i];
      if (k == "_") {
        default_mime = value;
        continue;
      }
      type_map[k] = value;
    }
  }
  return (true);
}

// ServerConfig method
bool WebserverConfig::is_ServerConfig(const std::string &line) {
  std::size_t i = 1;

  if (line.empty())
    return false;
  if (line[0] != ':')
    return false;
  if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i])))
    return false;
  while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])))
    ++i;
  if (i < line.size() && line[i] == ' ') {
    ++i;
    if (i < line.size() && line[i] == ' ')
      return false;
  }
  if (i >= line.size() || line[i] != '=')
    return false;
  ++i;
  return (i == line.size());
}

bool WebserverConfig::set_ServerConfig_map(std::ifstream &file,
                                           const std::string &line) {
  unsigned int key;
  std::string temp(line);
  ServerConfig config(file);

  key = parse_ServerConfig_key(temp);
  if (config.Geterr_line() != "") { // 서버블럭 확인
    err_meg = temp + " " + config.Geterr_line();
    return (false);
  }
  if (ServerConfig_map.find(key) != ServerConfig_map.end()) {
    err_meg = "Server block declared Error: " + line;
    return (false);
  }
  ServerConfig_map[key] = config;
  return (true);
}

unsigned int WebserverConfig::parse_ServerConfig_key(std::string &key) {
  std::size_t i = 1;
  std::size_t start = i;

  while (i < key.size() && std::isdigit(static_cast<unsigned char>(key[i])))
    ++i;
  return (atoi(key.substr(start, i - start).c_str()));
}
