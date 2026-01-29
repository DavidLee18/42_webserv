#ifndef WEBSERVERCONFIG_HPP
#define WEBSERVERCONFIG_HPP

#include "ServerConfig.hpp"

class WebserverConfig {
private:
  bool is_success;
  std::string err_meg;
  std::string default_mime;
  std::map<std::string, std::string> type_map;
  std::map<std::string, ServerConfig> ServerConfig_map; // key unsigned int로 변환

  bool file_parsing(std::ifstream &file);
  // type_map method
  bool set_type_map(std::ifstream &file);
  bool parse_type_line(const std::string &line,
                       std::vector<std::string> &keys_out,
                       std::string &value_out);
  std::vector<std::string> is_typeKey(const std::string &key);
  bool is_typeValue(const std::string &value);
  // ServerConfig method
  bool is_ServerConfig(const std::string &line);
  bool set_ServerConfig_map(std::ifstream &file, const std::string &line);
  std::string parse_ServerConfig_key(std::string &key);

  WebserverConfig(std::ifstream &file);
  ~WebserverConfig();
public:
  WebserverConfig(const WebserverConfig& other) : 
  default_mime(other.default_mime), type_map(other.type_map), ServerConfig_map(other.ServerConfig_map) {};


  static Result<WebserverConfig> parse(std::ifstream &file)
  {
    WebserverConfig temp(file);
    // OK
    if (temp.is_success)
      return OK(WebserverConfig, temp);
    return ERR(WebserverConfig, temp.err_meg);
  }
};



#endif
