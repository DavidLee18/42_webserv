#include "webserv.h"

int utils::count_occurrences(const std::string &line,
                             const std::string &delim) {
  int count = 0;
  std::string::size_type pos = 0;

  while (true) {
    pos = line.find(delim, pos);
    if (pos == std::string::npos)
      break;

    ++count;
    pos += delim.size();
  }
  return (count);
}

bool utils::has_invalid_char(const std::string &line,
                             const std::string &allowed) {

  for (std::size_t i = 0; i < line.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(line[i]);
    if (std::isalnum(c))
      continue;
    if (allowed.find(static_cast<char>(c)) != std::string::npos)
      continue;
    return (true);
  }
  return (false);
}

bool utils::has_space(const std::string &line) {
  const std::size_t pos = line.find(' ');

  if (pos == std::string::npos)
    return (false);
  return (true);
}

std::string utils::trim_whitespace(const std::string &s) {
  const std::size_t start = s.find_first_not_of(" \t");
  const std::size_t end = s.find_last_not_of(" \t");

  if (start == std::string::npos)
    return "";

  return s.substr(start, end - start + 1);
}

bool utils::match_indent_level(const std::string &line, const size_t num) {
  size_t len = 0;
  size_t i = 0;

  if (line.empty())
    return (num == 0);
  while (i < line.size() && line[i] == '\t') {
    len++;
    i++;
  }
  return (len == num);
}

std::vector<std::string> utils::string_split(const std::string &line,
                                             const std::string &delim) {
  std::vector<std::string> tokens;
  std::size_t start = 0;
  std::size_t end;

  while ((end = line.find(delim, start)) != std::string::npos &&
         !delim.empty()) {
    if (end > start)
      tokens.push_back(line.substr(start, end - start));
    start = end + delim.size();
  }
  if (start < line.size())
    tokens.push_back(line.substr(start));

  return tokens;
}

std::string utils::remove_char(std::string s, const char ch) {
  s.erase(std::remove(s.begin(), s.end(), ch), s.end());

  return (s);
}

std::string utils::join(const std::vector<std::string> &elements,
                        const std::string &delimiter) {
  std::stringstream ss;
  for (size_t i = 0; i < elements.size(); ++i) {
    if (i != 0) {
      ss << delimiter;
    }
    ss << elements[i];
  }
  return ss.str();
}
