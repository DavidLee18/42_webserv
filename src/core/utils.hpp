#ifndef UTILS_HPP
#define UTILS_HPP

#include "FileDescriptor.hpp"
#include "../server/Client.hpp"
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace utils {
enum LogLevel { DEBUG, INFO, WARNING, ERROR };
bool        has_space(const std::string& line);
std::string trim_whitespace(const std::string& s);
bool        match_indent_level(const std::string& line, size_t num);
std::vector<std::string> string_split(const std::string& line,
                                      const std::string& delim);
bool has_invalid_char(const std::string& line, const std::string& allowed);
int  count_occurrences(const std::string& line, const std::string& delim);
std::string   remove_char(std::string s, char ch);
std::string   join(const std::vector<std::string>& elements,
                   const std::string&              delimiter);
std::size_t   return_indent_level(std::string line);
bool          has_leading_space(const std::string& str);
bool          has_trailing_space(const std::string& str);
bool          is_header_name(const std::string& name);
bool          is_header_value(const std::string& value);
char          tolower(char c);
std::ostream& debug(std::ostream&);
std::ostream& info(std::ostream&);
std::ostream& warning(std::ostream&);
std::ostream& error(std::ostream&);
std::ostream& crlf(std::ostream&);
std::string   get_env(std::string const& name, char** envp);

unsigned char to_upper(unsigned char);
} // namespace utils

namespace configutils {
std::string  string_to_unsigned_int(const std::string& str, unsigned int& num);
std::string  get_indent_whitespace_error(const std::string& line, size_t level);
std::string  check_html_file(const std::string& path, char** envp);
Result<Void> prepare_config_line(std::string& origin_line, std::string& line,
                                 size_t level);
} // namespace configutils
#endif
