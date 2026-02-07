#ifndef WEBSERVERCONFIG_HPP
#define WEBSERVERCONFIG_HPP

#include "ServerConfig.hpp"

class WebserverConfig {
private:
  std::string err_meg;
  std::string default_mime;
  std::map<std::string, std::string> type_map;
  std::map<unsigned int, ServerConfig>
      ServerConfig_map; // key unsigned int로 변환

  bool file_parsing(FileDescriptor &file);
  // type_map method
  bool set_type_map(FileDescriptor &file);
  bool parse_type_line(const std::string &line,
                       std::vector<std::string> &keys_out,
                       std::string &value_out);
  std::vector<std::string> is_typeKey(const std::string &key);
  bool is_typeValue(const std::string &value);
  // ServerConfig method
  bool is_ServerConfig(const std::string &line);
  bool set_ServerConfig_map(FileDescriptor &file, const std::string &line);
  unsigned int parse_ServerConfig_key(std::string &key);

  WebserverConfig(FileDescriptor &file);

public:
  WebserverConfig(const WebserverConfig &other)
      : default_mime(other.default_mime), type_map(other.type_map),
        ServerConfig_map(other.ServerConfig_map) {};
  
  const std::string &Get_default_mime(void) const { return default_mime; }
  const std::map<std::string, std::string> &Get_Type_map(void) const { return type_map; }
  const std::map<unsigned int, ServerConfig> &Get_ServerConfig_map(void) const { return ServerConfig_map; }
  static Result<WebserverConfig> parse(FileDescriptor &file) {
    WebserverConfig temp(file);
    // OK
    if (temp.err_meg == "")
      return OK(WebserverConfig, temp);
    return ERR(WebserverConfig, temp.err_meg);
  }
};

std::ostream& operator<<(std::ostream& os, const WebserverConfig& data);

#endif
