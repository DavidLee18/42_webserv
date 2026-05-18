#include "WebserverConfig.hpp"

WebserverConfig::WebserverConfig(FileDescriptor &file, char **envp) {
  err_meg = "";
  count_line = 0;
  file_parsing(file, envp);
  if (err_meg != "") {
    if (!std::isdigit(static_cast<unsigned char>(err_meg[0])))
      err_meg = ConfigError::add_line_number(count_line, err_meg);
    return;
  }
  return;
}

bool WebserverConfig::file_parsing(FileDescriptor &file, char **envp) {
  std::string line;
  bool is_type_parse = false;
  bool is_cgi_parse = false;
  bool is_server_parse = false;
  bool is_err_page_parse = false;

  while (true) {
    Result<std::string> temp = file.read_file_line();
    count_line++;
    if (temp.error() != "") {
      err_meg = "FileDescriptor Error: " + temp.error();
      return false;
    } else if (temp.value() == "")
      break;
    else if (temp.value() == "\n")
      continue;

    origin_line = utils::remove_char(temp.value(), '\n');
    err_meg = configutils::get_indent_whitespace_error(origin_line, 0);
    if (err_meg != "")
      return false;
    line = utils::trim_whitespace(origin_line);

    if (line == "types =" || line == "types=") {
      if (is_type_parse == true) {
        err_meg = ConfigError::make(line, ERR_DUPLICATE_TYPE_BLOCK);
        return false;
      } else if (is_server_parse == true) {
        err_meg = ConfigError::make(line, ERR_INVALID_GLOBAL_BLOCK_LOCATION);
        return false;
      } else if (!parse_types_block(file))
        return false;
      is_type_parse = true;
    } else if (WebserverConfig::is_server_config_header(line)) {
      if (is_type_parse == false) {
        err_meg = ConfigError::add_line_number(1, ConfigError::make("", "", ERR_REQUIRED_TYPE_BLOCK_MISSING));
        return false;
      } else if (!parse_server_config_entry(file, line, envp))
        return false;
      is_server_parse = true;
    } else if (line == "cgi =" || line == "cgi=") {
      if (is_cgi_parse == true) {
        err_meg = ConfigError::make(line, ERR_DUPLICATE_GLOBAL_CGI_BLOCK);
        return false;
      } else if (is_server_parse == true) {
        err_meg = ConfigError::make(line, ERR_INVALID_GLOBAL_BLOCK_LOCATION);
        return false;
      }
      err_meg = RouteRule_CGI::parse_global_cgi_block(file, global_cgi,
                                                      count_line, envp);
      if (err_meg != "")
        return false;
      is_cgi_parse = true;
    } else if (line[0] == '!') {
      if (is_err_page_parse == true) {
        err_meg = ConfigError::make(line, ERR_DUPLICATE_DEFAULT_ERROR_PAGE_BLOCK);
        return false;
      } else if (is_server_parse == true) {
        err_meg = ConfigError::make(line, ERR_INVALID_GLOBAL_BLOCK_LOCATION);
        return false;
      }
      err_meg = ServerConfig::apply_err_page_entry(origin_line,
          line, default_err_page, envp);
      if (err_meg != "") 
        return false;
      is_err_page_parse = true;
    } else {
      err_meg = ConfigError::make(line, ERR_INVALID_TOP_LEVEL_FORMAT);
      return false;
    }
  }

  if (type_map.empty()) {
    err_meg = ConfigError::add_line_number(1, ConfigError::make("", "", ERR_REQUIRED_TYPE_BLOCK_MISSING));
    return false;
  } else if (serverconfig_map.empty()) {
    err_meg = ConfigError::add_line_number(1, ConfigError::make("", "", ERR_REQUIRED_SERVER_BLOCK_MISSING));
    return false;
  }
  return true;
}

std::vector<std::string>
WebserverConfig::parse_type_keys(const std::string &key) {
  int number_of_key = 0;
  std::string temp = key;
  std::vector<std::string> key_data;

  if (utils::has_invalid_char(temp, "_|")) {
    err_meg = ConfigError::make(origin_line, temp, ERR_INVALID_MIME_EXTENSION_CHAR);
    return (key_data);
  }

  key_data = utils::string_split(temp, "|");
  number_of_key = utils::count_occurrences(temp, "|") + 1;
  if (key_data.size() != static_cast<std::size_t>(number_of_key)) {
    err_meg = ConfigError::make(origin_line, temp, ERR_MIME_EXTENSION_COUNT_MISMATCH);
    return (std::vector<std::string>());
  }
  for (std::size_t i = 0; i < key_data.size(); ++i) {
    if (type_map.find(key_data[i]) != type_map.end()) {
      err_meg = ConfigError::make(origin_line, key_data[i], ERR_DUPLICATE_MIME_EXTENSION);
      return (std::vector<std::string>());
    }
  }
  return (key_data);
}

bool WebserverConfig::is_valid_mime_type(const std::string &value) {
  std::vector<std::string> value_data;

  if (utils::has_invalid_char(value, "/-")) {
    err_meg = ConfigError::make(origin_line, value, ERR_INVALID_MIME_TYPE_CHAR);
    return false;
  } else if (value[0] == '-' || value[0] == '/') {
    err_meg = ConfigError::make(origin_line, value, ERR_INVALID_MIME_TYPE_FORMAT);
    return false;
  }
  for (std::size_t i = 1; i < value.size(); ++i) {
    if (value[i] == '-' && value[i - 1] == '-') {
      err_meg = ConfigError::make(origin_line, value, ERR_INVALID_MIME_TYPE_FORMAT);
      return false;
    }
  }
  value_data = utils::string_split(value, "/");
  if (value_data.size() != 2) {
    err_meg = ConfigError::make(origin_line, value, ERR_INVALID_MIME_TYPE_FORMAT);
    return false;
  } else if (utils::count_occurrences(value, "/") != 1) {
    err_meg = ConfigError::make(origin_line, value, ERR_INVALID_MIME_TYPE_FORMAT);
    return false;
  }

  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '-') {
      if (i == value.size() - 1) {
        err_meg = ConfigError::make(origin_line, value, ERR_INVALID_MIME_TYPE_FORMAT);
        return false;
      }
      if (!std::isalnum(static_cast<unsigned char>(value[i - 1])) ||
          !std::isalnum(static_cast<unsigned char>(value[i + 1]))) {
        err_meg = ConfigError::make(origin_line, value, ERR_INVALID_MIME_TYPE_FORMAT);
        return false;
      }
    }
  }
  return true;
}

bool WebserverConfig::parse_type_mapping(const std::string &line,
                                         std::vector<std::string> &keys_out,
                                         std::string &value_out) {
  std::size_t pos = line.find("->");
  if (pos == std::string::npos) {
    err_meg = ConfigError::make(origin_line, line, ERR_MISSING_MIME_MAPPING_OPERATOR);
    return false;
  } else if (utils::count_occurrences(line, "->") != 1) {
    err_meg = ConfigError::make(origin_line, line, ERR_INVALID_MIME_MAPPING_SYNTAX);
    return false;
  }
  std::vector<std::string> type_data = utils::string_split(line, "->");
  if (type_data.size() != 2) {
    err_meg = ConfigError::make(origin_line, line, ERR_INVALID_MIME_MAPPING_VALUE);
    return false;
  }

  std::vector<std::string> keys =
      WebserverConfig::parse_type_keys(utils::trim_whitespace(type_data[0]));
  if (keys.empty() || !is_valid_mime_type(utils::trim_whitespace(type_data[1])))
    return false;
  err_meg = "";

  keys_out = keys;
  value_out = utils::trim_whitespace(type_data[1]);
  return true;
}

bool WebserverConfig::parse_types_block(FileDescriptor &file) {
  std::string line;
  std::string value;
  std::vector<std::string> keys;

  while (true) {
    Result<std::string> temp = file.read_file_line();
    count_line++;
    if (temp.error() != "") {
      err_meg = "FileDescriptor Error: " + temp.error();
      return (false);
    } else if (temp.value() == "\n" || temp.value() == "")
      break;

    origin_line = utils::remove_char(temp.value(), '\n');
    err_meg = configutils::get_indent_whitespace_error(origin_line, 1);
    if (err_meg != "")
      return false;
    line = utils::trim_whitespace(origin_line);

    if (!parse_type_mapping(line, keys, value))
      return false;

    for (std::size_t i = 0; i < keys.size(); ++i) {
      const std::string &k = keys[i];
      if (k == "_") {
        if (!default_mime.empty()) {
          err_meg = ConfigError::make(origin_line, value, ERR_DUPLICATE_DEFAULT_MIME_TYPE);
          return false;
        }
        default_mime = value;
        continue;
      } else if (type_map.find(k) != type_map.end()) {
        err_meg = ConfigError::make(origin_line, k, ERR_DUPLICATE_MIME_EXTENSION);
        return false;
      }
      type_map[k] = value;
    }
  }
  if (default_mime.empty()) {
    err_meg = ConfigError::make("", "", ERR_MISSING_DEFAULT_MIME_TYPE);
    return false;
  }
  return true;
}

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
                                                const std::string &line,
                                                char **envp) {
  unsigned int key = 0;
  std::string temp(line);
  ServerConfig server(file, global_cgi, envp);

  err_meg = configutils::string_to_unsigned_int(WebserverConfig::parse_server_port(temp), key);
  if (err_meg != "")
    err_meg = ConfigError::make(line, WebserverConfig::parse_server_port(temp), err_meg);
  if (MIN_PORT_VALUE > key || key > MAX_PORT_VALUE) {
    err_meg = ConfigError::make(line, WebserverConfig::parse_server_port(temp), ERR_INVALID_SERVER_PORT);
    return false;
  }

  if (server.get_err_meg() != "") {
    err_meg = server.get_err_meg();
    count_line += server.get_count_line();
    return false;
  } else if (server.get_routes().size() == 0 &&
             server.get_route_rule_cgi().size() == 0) {
    err_meg = ConfigError::make(origin_line, ERR_EMPTY_SERVER_BLOCK);
    return false;
  } else if (serverconfig_map.find(key) != serverconfig_map.end()) {
    std::ostringstream oss;
    oss << key;

    err_meg = ConfigError::make(line, oss.str(), ERR_DUPLICATE_SERVER_PORT);
    return false;
  }
  count_line += server.get_count_line();
  serverconfig_map[key] = server;
  return true;
}

std::string WebserverConfig::parse_server_port(const std::string &key) {
  std::size_t i = 1;
  std::size_t start = i;

  while (i < key.size() && std::isdigit(static_cast<unsigned char>(key[i])))
    ++i;
  return key.substr(start, i - start);
}

std::ostream &operator<<(std::ostream &os, const WebserverConfig &data) {
  const std::map<std::string, std::string> &ty = data.get_type_map();
  const std::map<std::string, std::string> &cgi = data.get_global_cgi();
  const std::map<unsigned int, std::string> &d_e = data.get_default_err_page();
  std::map<std::string, std::string>::const_iterator ty_it;
  std::map<std::string, std::string>::const_iterator cgi_it;

  os << utils::debug
     << "========================================================" << std::endl;
  os << utils::debug << "<<Type_map>>\n" << std::endl;
  for (ty_it = ty.begin(); ty_it != ty.end(); ++ty_it) {
    os << utils::debug << "Type key: " << ty_it->first
       << ", Type value: " << ty_it->second << std::endl;
  }
  os << utils::debug << "default_mime: " << data.get_default_mime()
     << std::endl;
  os << "<<Global CGI>>\n" << std::endl;
  for (cgi_it = cgi.begin(); cgi_it != cgi.end(); ++cgi_it) {
    os << "Global CGI key: " << cgi_it->first
       << " Global CGI value: " << cgi_it->second << " " << std::endl;
  }
  os << "========================================================" << std::endl;
  os << "\n\n\n========================================================"
     << std::endl;

  os << "<<DefaultErrPage>>\n" << std::endl;

  std::map<unsigned int, std::string>::const_iterator er_it;

  os << "\nerr_page\n";
  for (er_it = d_e.begin(); er_it != d_e.end(); ++er_it) {
    os << utils::debug << "\terr_page key: " << er_it->first
       << ", err_page value: " << er_it->second << std::endl;
  }
  os << utils::debug
     << "========================================================" << std::endl;
  os << "\n\n\n"
     << utils::debug
     << "========================================================" << std::endl;
  const std::map<unsigned int, ServerConfig> &Server_map =
      data.get_serverconfig_map();
  std::map<unsigned int, ServerConfig>::const_iterator Server_map_it;
  os << utils::debug << "<<Server_map>>" << std::endl;
  for (Server_map_it = Server_map.begin(); Server_map_it != Server_map.end();
       ++Server_map_it) {
    os << "\n"
       << utils::debug << "Server key: " << Server_map_it->first << std::endl;
    os << utils::debug << Server_map_it->second;
  }
  return (os);
}
