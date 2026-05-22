#ifndef RESPONSE_UTILS_HPP
#define RESPONSE_UTILS_HPP

#include "../config/ServerConfig.hpp"
#include "../core/Errors.hpp"
#include "Response.hpp"
#include <map>
#include <string>
#include <sys/stat.h>

class ServerResponse;

std::string get_string_from_map(const std::map<unsigned int, std::string> &map,
                                const int key);
std::string get_string_from_map(const std::map<std::string, std::string> &map,
                                const std::string &key);

class ResponseUtils {
public:
  static std::string find_file_type(const std::string &path);
  static std::string get_mime_type_for_extension(const std::string &ext);
  static std::string get_mime_type_for_extension(
      const std::string &ext,
      const std::map<std::string, std::string> &mime_map);
  static Result<int> check_path_type(const std::string &path);
  static Result<std::string> compute_etag(const std::string &path);
  static Result<std::string> get_last_modified(const std::string &path);
  static std::string escape_html(const std::string &input);
  static std::string get_http_date();
};

#endif
