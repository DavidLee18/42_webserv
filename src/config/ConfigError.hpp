#ifndef CONFIGERROR_HPP
#define CONFIGERROR_HPP

#include "../utils.hpp"

enum ConfigErrorCode {
  ERR_INVALID_SERVER_RESPONSE_TIME,
  ERR_TIMEOUT_DEFINED_AFTER_ROUTE,
  ERR_INVALID_FILE_PATH,
  ERR_NOT_REGULAR_FILE,
  ERR_FILE_NOT_READABLE,
  ERR_FILE_NOT_EXECUTABLE,
  ERR_DUPLICATED_SERVER_BLOCK,
  ERR_INVALID_ROUTE_RULE,
  ERR_UNKNOWN_CONFIG_ERROR
};

class ConfigError {
public:
  static std::string make(const std::string &line,
                          const std::string &target,
                          ConfigErrorCode code);

  static std::string make_without_target(const std::string &line,
                                         ConfigErrorCode code);

  static std::string simple(ConfigErrorCode code);

  static std::string add_line_number(std::size_t line_number,
                                     const std::string &message);

private:
  static std::string get_summary(ConfigErrorCode code);
  static std::string get_rule(ConfigErrorCode code);
  static std::string get_reason(ConfigErrorCode code);
  static std::string to_string(std::size_t value);
};

#endif