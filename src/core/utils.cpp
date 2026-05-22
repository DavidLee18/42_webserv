#include "utils.hpp"
#include <algorithm>
#include <cerrno>
#include <limits>

int utils::count_occurrences(const std::string& line,
                             const std::string& delim) {
    int                    count = 0;
    std::string::size_type pos   = 0;

    while (true) {
        pos = line.find(delim, pos);
        if (pos == std::string::npos) break;

        ++count;
        pos += delim.size();
    }
    return (count);
}

bool utils::has_invalid_char(const std::string& line,
                             const std::string& allowed) {

    for (std::size_t i = 0; i < line.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(line[i]);
        if (std::isalnum(c)) continue;
        if (allowed.find(static_cast<char>(c)) != std::string::npos) continue;
        return true;
    }
    return false;
}

bool utils::has_space(const std::string& line) {
    const std::size_t pos = line.find(' ');

    if (pos == std::string::npos) return (false);
    return (true);
}

std::string utils::trim_whitespace(const std::string& s) {
    const std::size_t start = s.find_first_not_of(" \t");
    const std::size_t end   = s.find_last_not_of(" \t");

    if (start == std::string::npos) return "";

    return s.substr(start, end - start + 1);
}

bool utils::match_indent_level(const std::string& line, const size_t num) {
    size_t len = 0;
    size_t i   = 0;

    if (line.empty()) return (num == 0);
    while (i < line.size() && line[i] == '\t') {
        len++;
        i++;
    }
    return (len == num);
}

std::size_t utils::return_indent_level(std::string line) {
    size_t len = 0;

    if (line.empty()) return (0);
    while (len < line.size() && line[len] == '\t') { len++; }
    return len;
}

std::vector<std::string> utils::string_split(const std::string& line,
                                             const std::string& delim) {
    std::vector<std::string> tokens;
    std::size_t              start = 0;
    std::size_t              end;

    while ((end = line.find(delim, start)) != std::string::npos &&
           !delim.empty()) {
        if (end > start) tokens.push_back(line.substr(start, end - start));
        start = end + delim.size();
    }
    if (start < line.size()) tokens.push_back(line.substr(start));

    return tokens;
}

std::string utils::remove_char(std::string s, const char ch) {
    s.erase(std::remove(s.begin(), s.end(), ch), s.end());

    return (s);
}

std::string utils::join(const std::vector<std::string>& elements,
                        const std::string&              delimiter) {
    std::stringstream ss;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i != 0) { ss << delimiter; }
        ss << elements[i];
    }
    return ss.str();
}

bool utils::has_leading_space(const std::string& str) {
    if (str.empty()) return false;

    return std::isspace(str[0]);
}

bool utils::has_trailing_space(const std::string& str) {
    if (str.empty()) return false;

    return std::isspace(str[str.length() - 1]);
}

bool utils::is_header_name(const std::string& name) {
    if (name.empty()) return false;
    const size_t not_header_name_pos =
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU"
                               "VWXYZ0123456789!#$%&\'*+-.^_`|~");
    return not_header_name_pos == std::string::npos;
}

bool utils::is_header_value(const std::string& value) {
    if (value.empty()) return true;
    for (std::string::const_iterator it = value.begin(); it != value.end();
         ++it) {
        const char c = *it;
        if ((c < 0x20 || c > 0x7e) && c != 0x09) return false;
    }
    return true;
}

char utils::tolower(const char c) {
    return static_cast<char>(std::tolower(static_cast<int>(c)));
}

std::ostream& utils::debug(std::ostream& os) { return os << "[DEBUG] "; }

std::ostream& utils::info(std::ostream& os) { return os << "[INFO] "; }

std::ostream& utils::warning(std::ostream& os) { return os << "[WARNING] "; }

std::ostream& utils::error(std::ostream& os) { return os << "[ERROR] "; }

std::ostream& utils::crlf(std::ostream& os) { return os << "\r" << std::endl; }

std::string   utils::get_env(std::string const& name, char** envp) {
    if (envp == NULL || *envp == NULL) return std::string();
    for (size_t i = 0; envp[i] != NULL; i++) {
        const std::string env_pair(envp[i]);
        const size_t      equals_pos = env_pair.find('=');
        if (equals_pos == std::string::npos) continue;
        if (name == env_pair.substr(0, equals_pos))
            return env_pair.substr(equals_pos + 1);
    }
    return std::string();
}

std::string configutils::get_indent_whitespace_error(const std::string &line,
                                                     size_t level) {
  std::size_t indent_level = utils::return_indent_level(line);
  std::string prefix = "on [" + line + "]: ";
  std::string err_line;

  if (line.empty()) {
    if (level == 0)
      return "";
    std::ostringstream l_oss;
    l_oss << level;
    return "on []: It is not a valid indentation level(expected indentation "
           "level: " +
           l_oss.str() + ", found: 0)";
  }
  if (level != 0 && line[0] != '\t') {
    err_line = prefix + "It is not a valid indentation character (expected "
                "indentation character: ['\\t'], found: [" +
                line[0] + "])";
  } else if ((level == 0 && std::isspace(line[0])) || indent_level != level) {
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
    i_oss << indent_level;

    err_line =
        prefix + "It is not a valid indentation level(expected indentation level: " +
        l_oss.str() + ", found: " + i_oss.str() + ")";
    return err_line;
  } else if (utils::has_leading_space(line.substr(level))) {
    err_line = prefix + "Leading whitespace exists.";
    return err_line;
  } else if (utils::has_trailing_space(line)) {
    err_line = prefix + "Trailing whitespace exists.";
    return err_line;
  }
  return err_line;
}

std::string configutils::string_to_unsigned_int(const std::string &str,
                                                unsigned int &num) {
  errno = 0;
  char *end;
  unsigned long temp = std::strtoul(str.c_str(), &end, 10);
  std::string prefix = "Invalid unsigned integer ";

  if (str.empty())
    return  prefix + "(value is empty.)";

  if (str[0] == '0') {
    if (str.size() != 1)
      return "(leading zeros are not allowed).";;
  } else if (*end != '\0')
    return prefix + "(value contains non-digit characters).";
  else if (errno == ERANGE || temp > std::numeric_limits<unsigned int>::max())
    return prefix + "(value is out of unsigned int range).";
  num = static_cast<unsigned int>(temp);
  return "";
}

std::string configutils::check_html_file(const std::string &path, char **envp) {

  std::string real_path = utils::get_env("PWD", envp) + "/" + path;
  std::string prefix = "Invalid HTML file ";
  struct stat st;
  if (stat(real_path.c_str(), &st) != 0)
    return prefix + "(file does not exist or cannot be accessed).";

  if (!S_ISREG(st.st_mode))
    return prefix + "(path is not a regular file).";

  if (real_path.length() < 5 ||
      real_path.substr(real_path.length() - 5) != ".html")
    return prefix + "(file extension must be .html).";

  if (access(real_path.c_str(), R_OK) != 0)
    return prefix + "(no read permission).";

  return "";
}

Result<Void> configutils::prepare_config_line(std::string &origin_line, std::string &line, size_t level) {
  origin_line = utils::remove_char(line, '\n');
  std::string err_meg = configutils::get_indent_whitespace_error(origin_line, level);
  if (err_meg != "")
    return ERR(Void, err_meg);
  line = utils::trim_whitespace(origin_line);
  return OKV;
}


unsigned char utils::to_upper(const unsigned char c) {
  return static_cast<unsigned char>(std::toupper(static_cast<int>(c)));
}