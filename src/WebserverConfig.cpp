#include "webserv.h"

WebserverConfig::WebserverConfig(FileDescriptor &file) {
  err_meg = "";
  if (!this->file_parsing(file)) {
    return;
  }
  return;
}

bool WebserverConfig::file_parsing(FileDescriptor &file) {
  std::string line;

  while (true) {
    Result<std::string> temp = file.read_file_line();
    if (temp.error() != "" || !utils::match_indent_level(temp.value(), 0)) {
      err_meg = "FileDescriptor Error: " + temp.error();
      if (temp.error() == "")
        err_meg =
            "Invalid line Error: " +
            utils::trim_whitespace(utils::remove_char(temp.value(), '\n'));
      return false;
    } else if (temp.value() == "")
      break;
    else if (temp.value() == "\n")
      continue;
    line = utils::remove_char(temp.value(), '\n');
    if (line == "types =" || line == "types=") {
      if (!parse_types_block(file))
        return false;
    } else if (WebserverConfig::is_server_config_header(line)) {
      if (!parse_server_config_entry(file, line))
        return false;
    } else if (line == "uwsgi =" || line == "uwsgi=") {
      err_meg = RouteRule_CGI::parse_uwsgi_block(file, this->uwsgi);
      if (err_meg != "")
        return false;
    } else {
      err_meg = "Invalid line Error: " + line;
      return false;
    }
  }
  if (this->type_map.empty() || this->default_mime.length() == 0) {
    err_meg = "Required type configuration is missing";
    return false;
  }
  return true;
}

// type_map method
std::vector<std::string> WebserverConfig::parse_type_keys(const std::string &key) {
  int number_of_key = 0;
  std::string temp = utils::trim_whitespace(key);
  std::vector<std::string> key_data;

  if (temp.empty() || utils::has_space(temp) ||
      utils::has_invalid_char(temp, "_|"))
    return (key_data);
  key_data = utils::string_split(temp, "|");
  number_of_key = utils::count_occurrences(temp, "|") + 1;
  if (key_data.size() != static_cast<std::size_t>(number_of_key))
    return (std::vector<std::string>());
  for (std::size_t i = 0; i < key_data.size(); ++i) {
    if (type_map.find(key_data[i]) != type_map.end())
      return (std::vector<std::string>());
  }
  return (key_data);
}

bool WebserverConfig::is_valid_mime_type(const std::string &value) {
  std::vector<std::string> value_data;

  if (value.empty())
    return (false);
  char last = value[value.length() - 1];
  if (!std::isalnum(static_cast<unsigned char>(last)))
    return (false);
  std::string temp = utils::trim_whitespace(value);
  if (temp.empty() || utils::has_space(temp) ||
      utils::has_invalid_char(temp, "/-"))
    return (false);
  for (std::size_t i = 1; i < temp.size(); ++i) {
    if (temp[i] == '-' && temp[i - 1] == '-')
      return (false);
  }
  value_data = utils::string_split(temp, "/");
  if (value_data.size() != 2 || utils::count_occurrences(temp, "/") != 1)
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

bool WebserverConfig::parse_type_mapping(const std::string &line,
                                      std::vector<std::string> &keys_out,
                                      std::string &value_out) {
  if (utils::count_occurrences(line, "->") != 1)
    return (false);
  std::vector<std::string> type_data = utils::string_split(line, "->");
  if (type_data.size() != 2)
    return (false);
  std::vector<std::string> keys = WebserverConfig::parse_type_keys(type_data[0]);
  if (keys.empty() || !is_valid_mime_type(type_data[1]))
    return (false);
  keys_out = keys;
  value_out = utils::trim_whitespace(type_data[1]);
  return (true);
}

bool WebserverConfig::parse_types_block(FileDescriptor &file) {
  std::string line;
  std::string value;
  std::vector<std::string> keys;

  while (true) {
    Result<std::string> temp = file.read_file_line();
    if (temp.error() != "") {
      err_meg = "FileDescriptor Error: " + temp.error();
      return (false);
    }
    if (temp.value() == "\n" || temp.value() == "")
      break;
    std::string raw = utils::remove_char(temp.value(), '\n');
    if (!utils::match_indent_level(temp.value(), 1) ||
    (!raw.empty() && (raw[raw.length() - 1] == ' ' || raw[raw.length() - 1] == '\t'))) {
      err_meg = "Type syntax Error: " +
                utils::trim_whitespace(utils::remove_char(temp.value(), '\n'));
      return (false);
    }
    line = utils::remove_char(temp.value(), '\n');
    if (!parse_type_mapping(line, keys, value)) {
      err_meg = "Type syntax Error: " + line;
      return (false);
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
      const std::string &k = keys[i];
      if (k == "_") {
      if (!default_mime.empty()) {
        err_meg = "Type syntax Error: duplicate default MIME type";
        return false;
      }  
        default_mime = value;
        continue;
      }
      type_map[k] = value;
    }
  }
  return (true);
}

// ServerConfig method
bool WebserverConfig::is_server_config_header(const std::string &line) {
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

bool WebserverConfig::parse_server_config_entry(FileDescriptor &file,
                                           const std::string &line) {
  unsigned int key;
  std::string temp(line);
  ServerConfig config(file);

  key = WebserverConfig::parse_server_port(temp);
  if (config.geterr_line() != "") {
    err_meg = temp + " " + config.geterr_line();
    return (false);
  }
  if (serverconfig_map.find(key) != serverconfig_map.end()) {
    err_meg = "Server block declared Error: " + line;
    return (false);
  }
  serverconfig_map[key] = config;
  return (true);
}

unsigned int WebserverConfig::parse_server_port(const std::string &key) {
  std::size_t i = 1;
  std::size_t start = i;

  while (i < key.size() && std::isdigit(static_cast<unsigned char>(key[i])))
    ++i;
  return static_cast<unsigned int>(
      std::atoi(key.substr(start, i - start).c_str()));
}

std::ostream &operator<<(std::ostream &os, const WebserverConfig &data) {
  const std::map<std::string, std::string> &ty = data.get_type_map();
  const std::map<std::string, std::string> &uw = data.get_uwsgi();
  std::map<std::string, std::string>::const_iterator ty_it;
  std::map<std::string, std::string>::const_iterator uw_it;

  os << "========================================================" << std::endl;
  os << "<<Type_map>>\n" << std::endl;
  for (ty_it = ty.begin(); ty_it != ty.end(); ++ty_it) {
    os << "Type key: " << ty_it->first << ", Type value: " << ty_it->second
       << std::endl;
  }
  os << "default_mime: " << data.get_default_mime() << std::endl;
  os << "========================================================" << std::endl;
  os << "\n\n\n========================================================"
     << std::endl;
  os << "<<Uwsgi>>\n" << std::endl;
  for (uw_it = uw.begin(); uw_it != uw.end(); ++uw_it) {
    os << "Uwsgi key: " << uw_it->first << ", Uwsgi value: " << uw_it->second
       << std::endl;
  }
  os << "========================================================" << std::endl;
  os << "\n\n\n========================================================"
     << std::endl;
  const std::map<unsigned int, ServerConfig> &Server_map =
      data.get_serverconfig_map();
  std::map<unsigned int, ServerConfig>::const_iterator Server_map_it;
  os << "<<Server_map>>" << std::endl;
  for (Server_map_it = Server_map.begin(); Server_map_it != Server_map.end();
       ++Server_map_it) {
    os << "\nServer key: " << Server_map_it->first << std::endl;
    os << Server_map_it->second;
  }
  return (os);
}
