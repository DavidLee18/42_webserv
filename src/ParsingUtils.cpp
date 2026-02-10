#include "webserv.h"

int number_of_delim(const std::string &line, const std::string &delim) {
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

bool is_have_special(const std::string &line, const std::string &allowed) {

  for (std::size_t i = 0; i < line.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(line[i]);
    if (std::isalnum(c))
      continue;
    if (allowed.find(static_cast<char>(c)) != std::string::npos)
      continue;
    return (true);
  }
  return (false);
}

bool is_have_space(const std::string &line) {
  std::size_t pos = line.find(' ');

  if (pos == std::string::npos)
    return (false);
  return (true);
}

std::string trim_space(const std::string &s) {
  std::size_t start = s.find_first_not_of(" \t");
  std::size_t end = s.find_last_not_of(" \t");

  if (start == std::string::npos)
    return "";

  return s.substr(start, end - start + 1);
}

bool is_tab_or_space(std::string line, size_t num) {
  size_t len = 0;
  size_t i = 0;

  if (line.empty())
    return (num == 0);
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
    if (line[i] == ' ')
      len += 1;
    else
      len += 4;
    i++;
  }
  return (len == (num * 4));
}

std::vector<std::string> string_split(const std::string &line,
                                      const std::string &delim) {
  std::vector<std::string> tokens;
  std::size_t start = 0;
  std::size_t end;

  while ((end = line.find(delim, start)) != std::string::npos &&
         delim.length() != 0) {
    if (end > start)
      tokens.push_back(line.substr(start, end - start));
    start = end + delim.size();
  }
  if (start < line.size())
    tokens.push_back(line.substr(start));

  return tokens;
}

std::string trim_char(std::string s, char ch) {
  s.erase(std::remove(s.begin(), s.end(), ch), s.end());

  return (s);
}
