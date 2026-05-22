#include "ConfigError.hpp"

struct ErrorInfo {
    ConfigErrorCode code;
    std::string     summary;
    std::string     rule;
    std::string     reason;

    ErrorInfo(ConfigErrorCode code, const std::string& summary,
              const std::string& rule, const std::string& reason)
        : code(code), summary(summary), rule(rule), reason(reason) {}
};

static const ErrorInfo g_error_infos[] = {
    ErrorInfo(ERR_AUTH_FILE_NOT_FOUND, "Invalid auth file path",
              "auth file path",
              "the value after '@' must refer to an existing file"),
    ErrorInfo(ERR_CGI_SCRIPT_MISSING_DOLLAR_PREFIX,
              "Invalid CGI script reference", "CGI syntax",
              "the script identifier must begin with '$' to be recognized as a "
              "valid CGI command"),
    ErrorInfo(
        ERR_DUPLICATE_CGI_ENV_KEY, "Duplicate CGI environment variable key",
        "CGI environment key uniqueness",
        "each key in a key=value pair must be unique within the same context"),
    ErrorInfo(ERR_DUPLICATE_DEFAULT_ERROR_PAGE_BLOCK,
              "Duplicate default error page block definition",
              "duplicate default error page block",
              "the default error page block has already been defined"),
    ErrorInfo(ERR_DUPLICATE_DEFAULT_MIME_TYPE,
              "Duplicate default MIME type detected",
              "default MIME type uniqueness",
              "the '_' default MIME type must be defined only once"),
    ErrorInfo(ERR_DUPLICATE_GLOBAL_CGI_BLOCK,
              "Duplicate global CGI block definition",
              "duplicate global CGI block",
              "the global CGI block has already been defined"),
    ErrorInfo(ERR_DUPLICATE_GLOBAL_CGI_MAPPING,
              "Duplicate global CGI mapping definition",
              "duplicate global CGI mapping",
              "the same file extension is already assigned to another "
              "executable path"),
    ErrorInfo(
        ERR_DUPLICATE_HEADER_BLOCK,
        "Duplicate header block definition in server block",
        "duplicate server header block",
        "the header block has already been defined in the same server block"),
    ErrorInfo(ERR_DUPLICATE_MIME_EXTENSION, "Duplicate MIME extension detected",
              "MIME extension uniqueness",
              "the same extension cannot be registered more than once"),
    ErrorInfo(ERR_DUPLICATE_SERVER_PORT, "Duplicate server block",
              "server uniqueness",
              "the same server port is declared more than once"),
    ErrorInfo(ERR_DUPLICATE_SERVER_TIMEOUT,
              "Duplicate timeout definition in server block",
              "duplicate server timeout",
              "the timeout has already been defined in the same server block"),
    ErrorInfo(ERR_DUPLICATE_TYPE_BLOCK, "Duplicate type block definition",
              "duplicate type block",
              "the type block has already been defined"),
    ErrorInfo(ERR_EMPTY_SERVER_BLOCK, "Invalid empty server block",
              "server block route rule requirement",
              "each server block must contain at least one RouteRule or one "
              "RouteRule_CGI"),
    ErrorInfo(ERR_FILE_NOT_EXECUTABLE, "Invalid executable permission",
              "executable file", "the file does not have execute permission"),
    ErrorInfo(ERR_FILE_NOT_READABLE, "Invalid file permission", "readable file",
              "the file does not have read permission"),
    ErrorInfo(ERR_HEADER_DEFINED_AFTER_ROUTE,
              "Invalid header definition location in server block",
              "server header definition order",
              "header must be defined before any RouteRule or RouteRule_CGI"),
    ErrorInfo(ERR_INVALID_CGI_ENV_EMPTY_KEY_OR_VALUE,
              "Invalid environment variable syntax", "key-value format",
              "the key or value is missing in the '(key=value)' declaration"),
    ErrorInfo(
        ERR_INVALID_CGI_ENV_FORMAT, "Invalid CGI environment variable format",
        "CGI environment variable format",
        "each assignment must contain exactly one '=' in the form KEY=VALUE"),
    ErrorInfo(ERR_INVALID_CGI_ENV_KEY, "Invalid CGI environment variable key",
              "CGI environment key",
              "the key must consist of uppercase letters, underscores, and "
              "digits not allowed at the first position"),
    ErrorInfo(ERR_INVALID_CGI_ENV_KEY_VALUE,
              "Invalid CGI environment key-value format",
              "CGI environment key-value",
              "the assignment must contain both a non-empty key and a "
              "non-empty value"),
    ErrorInfo(ERR_INVALID_CGI_ENV_MISSING_CLOSE_PAREN,
              "Invalid environment variable syntax",
              "environment variable format",
              "the '(key=value)' declaration is missing a closing ')'"),
    ErrorInfo(ERR_INVALID_CGI_ENV_MISSING_EQUAL,
              "Invalid environment variable syntax",
              "environment variable format",
              "the '(key=value)' declaration is missing '='"),
    ErrorInfo(
        ERR_INVALID_CGI_ENV_MULTIPLE_DECLARATIONS,
        "Invalid environment variable syntax",
        "environment variable declaration",
        "multiple inline environment variable declarations are not permitted"),
    ErrorInfo(ERR_INVALID_CGI_ENV_TRAILING_CHARS,
              "Invalid environment variable syntax",
              "environment variable format",
              "trailing characters were found after the closing ')'"),
    ErrorInfo(
        ERR_INVALID_CGI_EXTENSION_LINE, "Invalid CGI extended information line",
        "CGI extension",
        "the line must follow either key=value or ...<numeric string> format"),
    ErrorInfo(ERR_INVALID_CGI_INLINE_ENV_SYNTAX,
              "Invalid CGI environment variable syntax",
              "CGI inline environment",
              "characters after the executable path must start with '(' and "
              "follow the exact '(key=value)' format"),
    ErrorInfo(ERR_INVALID_CGI_TIMEOUT_RANGE, "Invalid CGI response time",
              "CGI response time",
              "the value must be between 1ms and 3600000ms inclusive"),
    ErrorInfo(ERR_INVALID_ERROR_PAGE_FORMAT, "Invalid error page format",
              "error page format",
              "the error page entry must contain exactly two tokens in the "
              "form '! status:path'"),
    ErrorInfo(ERR_INVALID_ERROR_PAGE_MAPPING, "Invalid error page mapping",
              "error page mapping",
              "the mapping must contain both a status code and a path in the "
              "form status:path and exactly one ':'"),
    ErrorInfo(
        ERR_INVALID_ERROR_PAGE_STATUS_CODE, "Invalid error page status code",
        "error page status code",
        "the status code must be a three-digit 4xx or 5xx HTTP error code"),
    ErrorInfo(ERR_INVALID_ERROR_PAGE_STATUS_CODE_FORMAT,
              "Invalid error page status code format",
              "error page status code format",
              "the status code must contain only digits"),
    ErrorInfo(ERR_INVALID_ERROR_PAGE_STATUS_CODE_RANGE,
              "Invalid error page status code range",
              "error page status code range",
              "the status code must be between 400 and 599 inclusive"),
    ErrorInfo(ERR_INVALID_FILE_PATH, "Invalid file path", "file existence",
              "the specified path does not exist or cannot be accessed"),
    ErrorInfo(ERR_INVALID_GLOBAL_BLOCK_LOCATION,
              "Invalid block definition location",
              "global block definition order",
              "non-server blocks must be defined before any server block"),
    ErrorInfo(ERR_INVALID_GLOBAL_CGI_EXECUTABLE_PATH_WHITESPACE,
              "Invalid executable path in global CGI mapping",
              "global CGI executable path",
              "the executable path contains whitespace"),
    ErrorInfo(ERR_INVALID_GLOBAL_CGI_EXTENSION_CHAR,
              "Invalid file extension in global CGI mapping",
              "global CGI file extension",
              "only alphanumeric characters, '-' and '_' are allowed"),
    ErrorInfo(ERR_INVALID_GLOBAL_CGI_EXTENSION_WHITESPACE,
              "Invalid file extension in global CGI mapping",
              "global CGI file extension",
              "the file extension contains whitespace"),
    ErrorInfo(ERR_INVALID_GLOBAL_CGI_MAPPING_SYNTAX,
              "Invalid global CGI mapping syntax", "global CGI mapping",
              "the mapping must contain exactly one '->' operator in the form "
              "extension -> executable"),
    ErrorInfo(ERR_INVALID_GLOBAL_CGI_MAPPING_VALUE,
              "Invalid global CGI mapping value", "global CGI mapping",
              "both the file extension and executable path are required"),
    ErrorInfo(ERR_INVALID_HEADER_COLON_COUNT, "Invalid HTTP header format",
              "HTTP header separator count",
              "a header must contain exactly one ':' separator in the form "
              "Header-Name: value"),
    ErrorInfo(ERR_INVALID_HEADER_FORMAT, "Invalid header format",
              "HTTP header format",
              "the header must follow the expected Header-Name: value format"),
    ErrorInfo(
        ERR_INVALID_HEADER_MISSING_COLON, "Invalid header format",
        "HTTP header separator",
        "a header must contain a ':' separator between header name and value"),
    ErrorInfo(ERR_INVALID_HEADER_NAME, "Invalid HTTP header name",
              "HTTP header name",
              "the header name is empty or contains illegal characters"),
    ErrorInfo(ERR_INVALID_HEADER_VALUE, "Invalid HTTP header value",
              "HTTP header value",
              "the header value contains non-printable or invalid control "
              "characters"),
    ErrorInfo(ERR_INVALID_MAX_BODY_SIZE_VALUE_SYNTAX,
              "Invalid max_body_size value syntax",
              "max_body_size value format",
              "the value after '->{}' must be a numeric string optionally "
              "followed by MB, MiB, KB, or KiB"),
    ErrorInfo(ERR_INVALID_MIME_EXTENSION_CHAR,
              "Invalid character in MIME extension mapping", "MIME extension",
              "only alphanumeric characters, '_' and '|' are allowed in the "
              "extension list"),
    ErrorInfo(ERR_INVALID_MIME_MAPPING_SYNTAX,
              "Invalid MIME type mapping syntax", "MIME type mapping",
              "the line must contain exactly one '->' operator in the form "
              "extension -> MIME type"),
    ErrorInfo(ERR_INVALID_MIME_MAPPING_VALUE, "Invalid MIME type mapping value",
              "MIME type mapping",
              "both the extension and MIME type are required in the form "
              "extension -> MIME type"),
    ErrorInfo(ERR_INVALID_MIME_TYPE_CHAR, "Invalid character in MIME type",
              "MIME type character",
              "only alphanumeric characters, '/' and '-' are allowed"),
    ErrorInfo(ERR_INVALID_MIME_TYPE_FORMAT, "Invalid MIME type format",
              "MIME type format",
              "the value must follow the required type/subtype structure"),
    ErrorInfo(ERR_INVALID_ROUTE_RULE_ADDITIONAL_INFO_FORMAT,
              "Invalid RouteRule additional information format",
              "RouteRule additional information format",
              "the line must contain exactly two tokens: KEY VALUE"),
    ErrorInfo(ERR_INVALID_ROUTE_RULE_ADDITIONAL_INFO_SYNTAX,
              "Invalid RouteRule additional information syntax",
              "RouteRule additional information syntax",
              "the line cannot be parsed as valid additional information; "
              "allowed keywords are '!', '@', '->{}', and '?'"),
    ErrorInfo(ERR_INVALID_SERVER_CONFIG_LINE,
              "Invalid server configuration line",
              "server configuration syntax",
              "the line does not match any valid server configuration syntax"),
    ErrorInfo(ERR_INVALID_SERVER_PORT, "Invalid server port",
              "port registered range",
              "the port value must be between 1024 and 49151 inclusive"),
    ErrorInfo(ERR_INVALID_SERVER_RESPONSE_TIME, "Invalid server response time",
              "server response time",
              "the value must be between 1ms and 60000ms inclusive"),
    ErrorInfo(ERR_INVALID_TOP_LEVEL_FORMAT, "Invalid configuration format",
              "top-level configuration grammar",
              "the line does not correspond to a valid grammar rule at "
              "indentation level 0"),
    ErrorInfo(ERR_INVALID_UNSIGNED_INT_LEADING_ZERO, "Invalid value",
              "unsigned integer leading zero",
              "the value must not contain leading zeros"),
    ErrorInfo(ERR_INVALID_WILDCARD_MAPPING,
              "Invalid wildcard mapping in route rule", "wildcard mapping",
              "each wildcard used on the left-hand path must be mapped to an "
              "independent '*' segment on the right-hand path"),
    ErrorInfo(ERR_MAX_BODY_SIZE_OUT_OF_RANGE, "Invalid max body size",
              "max body size range",
              "the max body size must not exceed 1024MiB or 1048576KB"),
    ErrorInfo(ERR_MIME_EXTENSION_COUNT_MISMATCH,
              "Mismatch between expected and actual number of extension items",
              "MIME extension count",
              "the number of extensions must equal the number of '|' "
              "separators plus one"),
    ErrorInfo(
        ERR_MISSING_DEFAULT_MIME_TYPE, "Missing default MIME type definition",
        "default MIME type requirement",
        "the '_' entry must be defined exactly once as the default MIME type"),
    ErrorInfo(ERR_MISSING_GLOBAL_CGI_MAPPING_OPERATOR,
              "Missing global CGI mapping operator",
              "global CGI mapping operator",
              "the mapping must contain the '->' operator in the form "
              "extension -> executable"),
    ErrorInfo(ERR_MISSING_MIME_MAPPING_OPERATOR,
              "Missing MIME type mapping operator",
              "MIME type mapping operator",
              "the mapping must contain the '->' operator in the form "
              "extension -> MIME type"),
    ErrorInfo(ERR_NOT_REGULAR_FILE, "Invalid file type", "regular file",
              "the given path is not a regular file"),
    ErrorInfo(ERR_REQUIRED_SERVER_BLOCK_MISSING,
              "Required server block is missing", "required server block",
              "at least one server block must be present in the configuration"),
    ErrorInfo(ERR_REQUIRED_TYPE_BLOCK_MISSING, "Required type block is missing",
              "required type block",
              "the types block must be defined before server blocks"),
    ErrorInfo(ERR_TIMEOUT_DEFINED_AFTER_ROUTE,
              "Invalid timeout definition location in server block",
              "server timeout definition order",
              "timeout must be defined before any RouteRule or RouteRule_CGI"),
    ErrorInfo(ERR_UNDEFINED_GLOBAL_CGI_EXTENSION,
              "Undefined file extension in global CGI mapping",
              "global CGI file extension",
              "the provided file extension is not defined in the allowed "
              "extension list"),
    ErrorInfo(ERR_UNDEFINED_ROUTE_OPERATOR, "Undefined route operator",
              "route rule operator",
              "the operator is not registered in the supported operation set"),
    ErrorInfo(ERR_UNKNOWN_CONFIG_ERROR, "Unknown config error", "unknown",
              "an unknown configuration validation error occurred")};

static const ErrorInfo& find_error_info(ConfigErrorCode code) {
    const std::size_t count = sizeof(g_error_infos) / sizeof(g_error_infos[0]);

    for (std::size_t i = 0; i < count; ++i) {
        if (g_error_infos[i].code == code) return g_error_infos[i];
    }
    return g_error_infos[count - 1];
}

std::string ConfigError::make(const std::string& line, ConfigErrorCode code) {
    const ErrorInfo& info = find_error_info(code);

    return "on [" + line + "]: " + info.summary + " (the " + info.rule +
           " rule is violated because " + info.reason + ").";
}

std::string ConfigError::make(const std::string& line,
                              const std::string& target, ConfigErrorCode code) {
    const ErrorInfo& info = find_error_info(code);

    return "on [" + line + "], [" + target + "]: " + info.summary + " (the " +
           info.rule + " rule is violated because " + info.reason + ").";
}

std::string ConfigError::make(const std::string& line,
                              const std::string& target,
                              const std::string& message) {
    return "on [" + line + "], [" + target + "]: " + message;
}

std::string ConfigError::make(const std::string& line,
                              const std::string& message) {
    return "on [" + line + "]: " + message;
}

std::string ConfigError::file_descriptor(const std::string& reason) {
    return "FileDescriptor Error: " + reason;
}

std::string ConfigError::add_line_number(std::size_t        line_number,
                                         const std::string& message) {
    return to_string(line_number) + " " + message;
}

std::string ConfigError::to_string(std::size_t value) {
    std::ostringstream oss;

    oss << value;
    return oss.str();
}
