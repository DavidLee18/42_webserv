// #include "webserv.h"
#include "WebserverConfig.hpp"

WebserverConfig::WebserverConfig(FileDescriptor &file) {
  err_meg = "";
  count_line = 0;
  if (!file_parsing(file)) {
    std::ostringstream oss;
    oss << count_line;

    if (!std::isdigit(static_cast<unsigned char>(err_meg[0])))
      err_meg = oss.str() + " " + err_meg;
    return;
  }
}

bool WebserverConfig::file_parsing(FileDescriptor &file) {

  while (true) {
    Result<std::string> temp = file.read_file_line();
    count_line++;
    if (temp.error() != "") {
      err_meg = "FileDescriptor Error: " + temp.error();
      return false;
    } else if (temp.value().empty())
      break;
    else if (temp.value() == "\n")
      continue;

    std::string line(utils::remove_char(temp.value(), '\n'));
    err_meg = utils::get_indent_whitespace_error(line, 0);
    if (err_meg != "")
      return false;
    line = utils::trim_whitespace(line);

    if (line == "types =" || line == "types=") {
      if (!parse_types_block(file))
        return false;
    } else if (WebserverConfig::is_server_config_header(line)) {
      if (!parse_server_config_entry(file, line))
        return false;
    } else if (line == "uwsgi =" || line == "uwsgi=") {
      err_meg = RouteRule_CGI::parse_uwsgi_block(file, uwsgi, count_line);
      if (err_meg != "")
        return false;
    } else if (line[0] == '!') {
      err_meg =
          ServerConfig::apply_default_err_page_entry(line, default_err_page);
      if (err_meg != "") {
        err_meg = "on [" + line + err_meg;
        return false;
      }
    } else {
      err_meg = "on [" + line +
                "]: Invalid configuration format (the line does not correspond "
                "to a valid grammar rule at indentation level 0).";
      return false;
    }
  }
  if (type_map.empty()) {
    err_meg = "1 on [], []: Required type block is missing (the 'types' block "
              "is not defined at indentation level 0, so no MIME type mapping "
              "rules can be processed).";
    return false;
  } else if (serverconfig_map.empty()) {
    err_meg = "1 on [], []: Required server block is missing. (the Server "
              "block is mandatory but not present in the configuration).";
    return false;
  }
  return true;
}

// type_map method
std::vector<std::string>
WebserverConfig::parse_type_keys(const std::string &key) {
  int number_of_key = 0;
  std::string temp = key;
  std::vector<std::string> key_data;

  if (utils::has_invalid_char(temp, "_|")) {
    err_meg += temp +
               "]: Invalid character in extension part of header (only '|' and "
               "'_' are allowed as special characters within the extension, "
               "the extension contains disallowed characters).";
    return (key_data);
  }
  key_data = utils::string_split(temp, "|");
  number_of_key = utils::count_occurrences(temp, "|") + 1;
  if (key_data.size() != static_cast<std::size_t>(number_of_key)) {
    std::ostringstream key_num;
    std::ostringstream num;

    key_num << key_data.size();
    num << number_of_key;

    err_meg += temp +
               "]: Mismatch between expected and actual number of extension "
               "items (expected count: number of extensions must equal the "
               "number of '|' separators plus one, expected items: " +
               num.str() + ", actual items: " + key_num.str() + ").";
    return (std::vector<std::string>());
  }
  for (std::size_t i = 0; i < key_data.size(); ++i) {
    if (type_map.find(key_data[i]) != type_map.end()) {
      err_meg += temp +
                 "]: Duplicate extensions detected (the same extension was "
                 "registered more than once, violating the rule that the same "
                 "extension cannot be registered multiple times).";
      return (std::vector<std::string>());
    }
  }
  return (key_data);
}

bool WebserverConfig::is_valid_mime_type(const std::string &value) {

  if (utils::has_invalid_char(value, "/-")) {
    err_meg += value +
               "]: Invalid character in MIME type part of header (only '/' "
               "and '_' are allowed as special characters within the MIME "
               "type, the MIME type contains disallowed characters).";
    return false;
  } else if (value[0] == '-') {
    err_meg += value +
               "]: Invalid MIME type format (the type part of the MIME type "
               "must not start with '-', as it violates the rule that the "
               "type/subtype structure must start with a valid type name).";
    return false;
  } else if (value[0] == '/') {
    err_meg += value +
               "]: Invalid MIME type format (MIME type must not start with "
               "'/', as it violates the required 'type/subtype' structure).";
    return false;
  }
  for (std::size_t i = 1; i < value.size(); ++i) {
    if (value[i] == '-' && value[i - 1] == '-') {
      err_meg += value +
                 "]: Invalid MIME type format (consecutive '-' characters are "
                 "not allowed in the MIME type, as they violate the naming "
                 "rules for a valid type/subtype structure).";
      return false;
    }
  }
  const std::vector<std::string> value_data = utils::string_split(value, "/");
  if (value_data.size() != 2) {
    err_meg += value + "]: Invalid MIME type format (the value does not follow "
                       "the required 'type/subtype' structure).";
    return false;
  } else if (utils::count_occurrences(value, "/") != 1) {
    err_meg += value + "]: Invalid MIME type format (multiple '/' characters "
                       "are not allowed; a valid MIME type must contain "
                       "exactly one '/' separating type and subtype).";
    return false;
  }

  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '-') {
      if (i == value.size() - 1) {
        err_meg +=
            value +
            "]: Invalid MIME type format (the type part of the MIME type must "
            "not end with '-', as it violates the rule that the type/subtype "
            "structure must start with a valid type name).";
        return false;
      }
      if (!std::isalnum(static_cast<unsigned char>(value[i - 1])) ||
          !std::isalnum(static_cast<unsigned char>(value[i + 1]))) {
        err_meg +=
            value +
            "]: Invalid MIME type format (the type part must consist only of "
            "letters and digits; hyphens, whitespace, underscores, and other "
            "special characters are not allowed).";
        return false;
      }
    }
  }
  return true;
}

bool WebserverConfig::parse_type_mapping(const std::string &line,
                                         std::vector<std::string> &keys_out,
                                         std::string &value_out) {
  if (utils::count_occurrences(line, "->") != 1) {
    err_meg = "on [\t" + line +
              "]: Missing '->' in header (violates the rule requiring the "
              "'extension -> MIME type' format, so the mapping between "
              "extension and MIME type cannot be determined).";
    return false;
  }
  std::vector<std::string> type_data = utils::string_split(line, "->");
  if (type_data.size() != 2) {
    err_meg =
        "on [\t" + line +
        "]: Too many elements in header (violates the 'extension -> MIME type' "
        "format by including extra tokens beyond the required two components).";
    return false;
  }

  err_meg = "on [\t" + line + "], [";
  std::vector<std::string> keys =
      WebserverConfig::parse_type_keys(utils::trim_whitespace(type_data[0]));
  if (keys.empty() || !is_valid_mime_type(utils::trim_whitespace(type_data[1])))
    return false;
  err_meg = "";

  keys_out = keys;
  value_out = utils::trim_whitespace(type_data[1]);
  return true;
}

bool WebserverConfig::parse_types_block(const FileDescriptor &file) {
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

    std::string line(utils::remove_char(temp.value(), '\n'));
    err_meg = utils::get_indent_whitespace_error(line, 1);
    if (err_meg != "")
      return false;
    line = utils::trim_whitespace(line);

    if (!parse_type_mapping(line, keys, value))
      return false;

    for (std::size_t i = 0; i < keys.size(); ++i) {
      const std::string &k = keys[i];
      if (k == "_") {
        if (!default_mime.empty()) {
          err_meg = "on [\t" + line + "], [" + value +
                    "]: Duplicate default MIME type detected (the default MIME "
                    "type '_' must be defined only once; multiple declarations "
                    "are not allowed).";
          return false;
        }
        default_mime = value;
        continue;
      }
      type_map[k] = value;
    }
  }
  if (default_mime.empty()) {
    err_meg = "on [], []: Missing default MIME type definition (the '_' entry "
              "must be defined exactly once as the default MIME type).";
    return false;
  }
  return true;
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
  ServerConfig sever(file);

  key = WebserverConfig::parse_server_port(temp);
  if (sever.get_err_meg() != "") {
    err_meg = sever.get_err_meg();
    count_line += sever.get_count_line();
    return false;
  }
  if (serverconfig_map.find(key) != serverconfig_map.end()) {
    std::ostringstream oss;
    oss << key;

    err_meg = "on [\t" + line + "], [" + oss.str() +
              "]:Violates configuration rule (server block is declared more "
              "than once).";
    return false;
  }
  serverconfig_map[key] = sever;
  return true;
}

unsigned int WebserverConfig::parse_server_port(const std::string &key) {
  std::size_t i = 1;
  const std::size_t start = i;

  while (i < key.size() && std::isdigit(static_cast<unsigned char>(key[i])))
    ++i;
  return static_cast<unsigned int>(
      std::atoi(key.substr(start, i - start).c_str()));
}

std::ostream &operator<<(std::ostream &os, const WebserverConfig &data) {
  const std::map<std::string, std::string> &ty = data.get_type_map();
  const std::map<int, RouteRule_CGI> &uw = data.get_uwsgi();
  const std::map<int, std::string> &d_e = data.get_default_err_page();
  std::map<std::string, std::string>::const_iterator ty_it;
  std::map<int, RouteRule_CGI>::const_iterator uw_it;

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
    os << "Uwsgi key: " << uw_it->first << "\nUwsgi value:\n"
       << uw_it->second << std::endl;
  }
  os << "========================================================" << std::endl;
  os << "\n\n\n========================================================"
     << std::endl;

  os << "<<DefaultErrPage>>\n" << std::endl;

  std::map<int, std::string>::const_iterator er_it;

  os << "\nerr_page\n";
  for (er_it = d_e.begin(); er_it != d_e.end(); ++er_it) {
    os << "\terr_page key: " << er_it->first
       << ", err_page value: " << er_it->second << std::endl;
  }
  os << "========================================================" << std::endl;
  os << "\n\n\n========================================================"
     << std::endl;
  const std::map<unsigned int, ServerConfig> &Server_map =
      data.get_serverconfig_map();
  os << "<<Server_map>>" << std::endl;
  for (std::map<unsigned int, ServerConfig>::const_iterator Server_map_it =
           Server_map.begin();
       Server_map_it != Server_map.end(); ++Server_map_it) {
    os << "\nServer key: " << Server_map_it->first << std::endl;
    os << Server_map_it->second;
  }
  return (os);
}
