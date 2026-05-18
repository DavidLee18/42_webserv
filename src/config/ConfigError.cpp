#include "ConfigError.hpp"

#include <sstream>

struct ErrorInfo {
  ConfigErrorCode code;
  const char *summary;
  const char *rule;
  const char *reason;
};

static const ErrorInfo g_error_infos[] = {
    {
        ERR_INVALID_SERVER_RESPONSE_TIME,
        "Invalid server response time",
        "server response time",
        "the value must be between 1ms and 60000ms inclusive",
    },
    {
        ERR_TIMEOUT_DEFINED_AFTER_ROUTE,
        "Invalid timeout definition location in server block",
        "server timeout definition order",
        "timeout must be defined before any RouteRule or RouteRule_CGI",
    },
    {
        ERR_INVALID_FILE_PATH,
        "Invalid file path",
        "file existence",
        "the specified path does not exist or cannot be accessed",
    },
    {
        ERR_NOT_REGULAR_FILE,
        "Invalid file type",
        "regular file",
        "the given path is not a regular file",
    },
    {
        ERR_FILE_NOT_READABLE,
        "Invalid file permission",
        "readable file",
        "the file does not have read permission",
    },
    {
        ERR_FILE_NOT_EXECUTABLE,
        "Invalid executable permission",
        "executable file",
        "the file does not have execute permission",
    },
    {
        ERR_DUPLICATED_SERVER_BLOCK,
        "Duplicated server block",
        "server uniqueness",
        "the same server configuration is already defined",
    },
    {
        ERR_INVALID_ROUTE_RULE,
        "Invalid route rule",
        "route rule format",
        "the route rule does not match the required configuration format",
    },
    {
        ERR_UNKNOWN_CONFIG_ERROR,
        "Unknown config error",
        "unknown",
        "an unknown configuration validation error occurred",
    },
};

static const ErrorInfo &find_error_info(ConfigErrorCode code) {
  const std::size_t count = sizeof(g_error_infos) / sizeof(g_error_infos[0]);

  for (std::size_t i = 0; i < count; ++i) {
    if (g_error_infos[i].code == code)
      return g_error_infos[i];
  }

  return g_error_infos[count - 1];
}

std::string ConfigError::make(const std::string &line,
                              const std::string &target,
                              ConfigErrorCode code) {
  const ErrorInfo &info = find_error_info(code);

  return "on [" + line + "], [" + target + "]: " + info.summary +
         " (the " + info.rule + " rule is violated because " + info.reason +
         ").";
}

std::string ConfigError::make_without_target(const std::string &line,
                                             ConfigErrorCode code) {
  const ErrorInfo &info = find_error_info(code);

  return "on [" + line + "]: " + info.summary + " (the " + info.rule +
         " rule is violated because " + info.reason + ").";
}

std::string ConfigError::simple(ConfigErrorCode code) {
  const ErrorInfo &info = find_error_info(code);

  return std::string(info.summary) + " (the " + info.rule +
         " rule is violated because " + info.reason + ").";
}

std::string ConfigError::add_line_number(std::size_t line_number,
                                         const std::string &message) {
  return to_string(line_number) + " " + message;
}

std::string ConfigError::get_summary(ConfigErrorCode code) {
  return find_error_info(code).summary;
}

std::string ConfigError::get_rule(ConfigErrorCode code) {
  return find_error_info(code).rule;
}

std::string ConfigError::get_reason(ConfigErrorCode code) {
  return find_error_info(code).reason;
}

std::string ConfigError::to_string(std::size_t value) {
  std::ostringstream oss;

  oss << value;
  return oss.str();
}