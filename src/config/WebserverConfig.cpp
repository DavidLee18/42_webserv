#include "WebserverConfig.hpp"

WebserverConfig::WebserverConfig(FileDescriptor& file, char** envp) {
    err_meg             = "";
    count_line          = 0;
    Result<Void> result = file_parsing(file, envp);
    if (!result.error().empty()) {
        std::ostringstream oss;
        oss << count_line;

        err_meg = result.error();
        if (!err_meg.empty() &&
            !std::isdigit(static_cast<unsigned char>(err_meg[0])))
            err_meg = oss.str() + " " + err_meg;
        return;
    }
}

Result<Void> WebserverConfig::file_parsing(FileDescriptor& file, char** envp) {
    std::string line;
    bool        is_type_parse   = false;
    bool        is_cgi_parse    = false;
    bool        is_server_parse = false;

  while (true) {
    count_line++;
    TRY(Void, std::string, line, file.read_file_line())
    if (line == "")
      break;
    else if (line == "\n")
      continue;
    TRY_(Void, Void, configutils::prepare_config_line(origin_line, line, 0))

        if (line == "types =" || line == "types=") {
            if (is_type_parse == true)
                return ERR(Void, ConfigError::make(origin_line,
                                                   ERR_DUPLICATE_TYPE_BLOCK));
            else if (is_server_parse == true)
                return ERR(
                    Void, ConfigError::make(origin_line,
                                            ERR_INVALID_GLOBAL_BLOCK_LOCATION));

            TRY_(Void, Void, parse_types_block(file))
            is_type_parse = true;
        } else if (WebserverConfig::is_server_config_header(line)) {
            if (is_type_parse == false)
                return ERR(
                    Void, ConfigError::add_line_number(
                              1, ConfigError::make(
                                     "", "", ERR_REQUIRED_TYPE_BLOCK_MISSING)));

            TRY_(Void, Void, parse_server_config_entry(file, line, envp))
            is_server_parse = true;
        } else if (line == "cgi =" || line == "cgi=") {
            if (is_cgi_parse == true)
                return ERR(Void,
                           ConfigError::make(origin_line,
                                             ERR_DUPLICATE_GLOBAL_CGI_BLOCK));
            else if (is_server_parse == true)
                return ERR(
                    Void, ConfigError::make(origin_line,
                                            ERR_INVALID_GLOBAL_BLOCK_LOCATION));

            TRY_(Void, Void,
                 RouteRule_CGI::parse_global_cgi_block(file, global_cgi,
                                                       count_line, envp))
            is_cgi_parse = true;
        } else
            return ERR(Void, ConfigError::make(origin_line,
                                               ERR_INVALID_TOP_LEVEL_FORMAT));
    }

    if (type_map.empty())
        return ERR(Void, ConfigError::add_line_number(
                             1, ConfigError::make(
                                    "", "", ERR_REQUIRED_TYPE_BLOCK_MISSING)));
    else if (serverconfig_map.empty())
        return ERR(
            Void, ConfigError::add_line_number(
                      1, ConfigError::make("", "",
                                           ERR_REQUIRED_SERVER_BLOCK_MISSING)));
    return OKV;
}

Result<std::vector<std::string> >
WebserverConfig::parse_type_keys(const std::string& key) {
    int                      number_of_key = 0;
    std::string              temp          = key;
    std::vector<std::string> key_data;

    if (utils::has_invalid_char(temp, "_|"))
        return ERR(std::vector<std::string>,
                   ConfigError::make(origin_line, temp,
                                     ERR_INVALID_MIME_EXTENSION_CHAR));

    key_data      = utils::string_split(temp, "|");
    number_of_key = utils::count_occurrences(temp, "|") + 1;
    if (key_data.size() != static_cast<std::size_t>(number_of_key))
        return ERR(std::vector<std::string>,
                   ConfigError::make(origin_line, temp,
                                     ERR_MIME_EXTENSION_COUNT_MISMATCH));
    for (std::size_t i = 0; i < key_data.size(); ++i) {
        if (type_map.find(key_data[i]) != type_map.end())
            return ERR(std::vector<std::string>,
                       ConfigError::make(origin_line, key_data[i],
                                         ERR_DUPLICATE_MIME_EXTENSION));
    }
    return OK(std::vector<std::string>, key_data);
}

Result<Void> WebserverConfig::is_valid_mime_type(const std::string& value) {
    std::vector<std::string> value_data;

    if (utils::has_invalid_char(value, "/-"))
        return ERR(Void, ConfigError::make(origin_line, value,
                                           ERR_INVALID_MIME_TYPE_CHAR));
    else if (value[0] == '-' || value[0] == '/')
        return ERR(Void, ConfigError::make(origin_line, value,
                                           ERR_INVALID_MIME_TYPE_FORMAT));

    for (std::size_t i = 1; i < value.size(); ++i)
        if (value[i] == '-' && value[i - 1] == '-') {
            return ERR(Void, ConfigError::make(origin_line, value,
                                               ERR_INVALID_MIME_TYPE_FORMAT));
        }

    value_data = utils::string_split(value, "/");
    if (value_data.size() != 2)
        return ERR(Void, ConfigError::make(origin_line, value,
                                           ERR_INVALID_MIME_TYPE_FORMAT));
    else if (utils::count_occurrences(value, "/") != 1)
        return ERR(Void, ConfigError::make(origin_line, value,
                                           ERR_INVALID_MIME_TYPE_FORMAT));

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '-') {
            if (i == value.size() - 1)
                return ERR(Void,
                           ConfigError::make(origin_line, value,
                                             ERR_INVALID_MIME_TYPE_FORMAT));

            if (!std::isalnum(static_cast<unsigned char>(value[i - 1])) ||
                !std::isalnum(static_cast<unsigned char>(value[i + 1])))
                return ERR(Void,
                           ConfigError::make(origin_line, value,
                                             ERR_INVALID_MIME_TYPE_FORMAT));
        }
    }
    return OKV;
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

Result<Void>
WebserverConfig::parse_type_mapping(const std::string&        line,
                                    std::vector<std::string>& keys_out,
                                    std::string&              value_out) {
    std::size_t pos = line.find("->");
    if (pos == std::string::npos)
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_MISSING_MIME_MAPPING_OPERATOR));
    else if (utils::count_occurrences(line, "->") != 1)
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_INVALID_MIME_MAPPING_SYNTAX));

    std::vector<std::string> type_data = utils::string_split(line, "->");
    if (type_data.size() != 2)
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_INVALID_MIME_MAPPING_VALUE));

    std::vector<std::string> keys;

    TRY(Void, std::vector<std::string>, keys,
        parse_type_keys(utils::trim_whitespace(type_data[0])))
    TRY_(Void, Void, is_valid_mime_type(utils::trim_whitespace(type_data[1])))

    keys_out  = keys;
    value_out = utils::trim_whitespace(type_data[1]);
    return OKV;
}

Result<Void> WebserverConfig::parse_types_block(FileDescriptor& file) {
    std::string              line;
    std::string              value;
    std::vector<std::string> keys;

  while (true) {
    count_line++;
    TRY(Void, std::string, line, file.read_file_line())
    if (line == "\n" || line == "")
      break;
    TRY_(Void, Void, configutils::prepare_config_line(origin_line, line, 1))

        TRY_(Void, Void, parse_type_mapping(line, keys, value))

        for (std::size_t i = 0; i < keys.size(); ++i) {
            const std::string& k = keys[i];
            if (k == "_") {
                if (!default_mime.empty())
                    return ERR(Void, ConfigError::make(
                                         origin_line, value,
                                         ERR_DUPLICATE_DEFAULT_MIME_TYPE));
                default_mime = value;
                continue;
            } else if (type_map.find(k) != type_map.end())
                return ERR(Void,
                           ConfigError::make(origin_line, k,
                                             ERR_DUPLICATE_MIME_EXTENSION));
            type_map[k] = value;
        }
    }
    if (default_mime.empty())
        return ERR(Void,
                   ConfigError::make("", "", ERR_MISSING_DEFAULT_MIME_TYPE));
    return OKV;
}

Result<Void> WebserverConfig::parse_server_config_entry(FileDescriptor&    file,
                                                        const std::string& line,
                                                        char** envp) {
    unsigned int key  = 0;
    std::string  port = WebserverConfig::parse_server_port(line);
    ServerConfig server(file, global_cgi, envp);

    err_meg = configutils::string_to_unsigned_int(port, key);
    if (err_meg != "")
        return ERR(Void, ConfigError::make(origin_line, port, err_meg));
    if (MIN_PORT_VALUE > key || key > MAX_PORT_VALUE)
        return ERR(Void, ConfigError::make(origin_line, port,
                                           ERR_INVALID_SERVER_PORT));

    if (server.get_err_meg() != "") {
        count_line += server.get_count_line();
        return ERR(Void, server.get_err_meg());
    } else if (server.get_routes().size() == 0 &&
               server.get_route_rule_cgi().size() == 0) {
        return ERR(Void,
                   ConfigError::make(origin_line, ERR_EMPTY_SERVER_BLOCK));
    } else if (serverconfig_map.find(key) != serverconfig_map.end()) {
        std::ostringstream oss;
        oss << key;

        return ERR(Void, ConfigError::make(origin_line, oss.str(),
                                           ERR_DUPLICATE_SERVER_PORT));
    }
    count_line += server.get_count_line();
    serverconfig_map[key] = server;
    return OKV;
}

std::string WebserverConfig::parse_server_port(const std::string& key) {
    std::size_t i     = 1;
    std::size_t start = i;

    while (i < key.size() && std::isdigit(static_cast<unsigned char>(key[i])))
        ++i;
    return key.substr(start, i - start);
}

std::ostream& operator<<(std::ostream& os, const WebserverConfig& data) {
    const std::map<std::string, std::string>& ty  = data.get_type_map();
    const std::map<std::string, std::string>& cgi = data.get_global_cgi();
    std::map<std::string, std::string>::const_iterator ty_it;
    std::map<std::string, std::string>::const_iterator cgi_it;

    os << utils::debug
       << "========================================================"
       << std::endl;
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
    os << "========================================================"
       << std::endl;
    os << "\n\n\n"
       << utils::debug
       << "========================================================"
       << std::endl;
    const std::map<unsigned int, ServerConfig>& Server_map =
        data.get_serverconfig_map();
    std::map<unsigned int, ServerConfig>::const_iterator Server_map_it;
    os << utils::debug << "<<Server_map>>" << std::endl;
    for (Server_map_it = Server_map.begin(); Server_map_it != Server_map.end();
         ++Server_map_it) {
        os << "\n"
           << utils::debug << "Server key: " << Server_map_it->first
           << std::endl;
        os << utils::debug << Server_map_it->second;
    }
    return (os);
}
