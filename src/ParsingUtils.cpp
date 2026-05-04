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

std::size_t utils::return_indent_level(std::string line) {
  size_t len = 0;

  if (line.empty())
    return (0);
  while (len < line.size() && line[len] == '\t') {
    len++;
  }
  return len;
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

bool utils::has_leading_space(const std::string& str)
{
    if (str.empty())
        return false;

    return std::isspace(str[0]);
}

bool utils::has_trailing_space(const std::string& str)
{
    if (str.empty())
        return false;

    return std::isspace(str[str.length() - 1]);
}

std::string utils::get_indent_whitespace_error(const std::string& line, size_t level) {
  std::size_t indent_level = utils::return_indent_level(line);
  std::string err_line = "";


  if (level != 0 && line[0] != '\t') {
    err_line += "on [" + line + "]: It is not a valid indentation character (expected indentation character: ['\\t'], found: [" + line[0] +"])";
  }
  else if ((level == 0 && std::isspace(line[0])) || indent_level != level) {
    std::ostringstream i_oss;
    std::ostringstream l_oss;

    l_oss << level;
    if (level == 0) {
      for (std::size_t i = 0; i < line.size(); i++) {
        indent_level = i;
        if (!std::isspace(line[i]))
          break;
      }
    }
    i_oss << indent_level;;

    err_line += "on [" + line + "]: It is not a valid indentation level(expected indentation level: " + l_oss.str() + ", found: " + i_oss.str() +")";
    return err_line;
  } else if (utils::has_leading_space(&line[level])) {
    err_line += "on [" + line + "]: Leading whitespace exists.";
    return err_line;
  } else if (utils::has_trailing_space(line)) {
    err_line += "on [" + line + "]: Trailing whitespace exists.";
    return err_line;
  }
  return err_line;
}

std::string utils::check_html_file(const std::string &path)
{
    char cwd[4096];
    getcwd(cwd, sizeof(cwd));

    std::string real_path = std::string(cwd) + path;
    struct stat st;

    if (stat(real_path.c_str(), &st) != 0)
        return "Invalid HTML file (file does not exist or cannot be accessed).";

    if (!S_ISREG(st.st_mode))
        return "Invalid HTML file (path is not a regular file).";

    if (real_path.length() < 5 || real_path.substr(real_path.length() - 5) != ".html")
        return "Invalid HTML file (file extension must be .html).";

    if (access(real_path.c_str(), R_OK) != 0)
        return "Invalid HTML file (no read permission).";

    return "";
}