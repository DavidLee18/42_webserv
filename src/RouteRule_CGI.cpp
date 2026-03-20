#include "RouteRule_CGI.hpp"

RouteRule_CGI::RouteRule_CGI(FileDescriptor &fd, std::string line) {
  err = "";
  timeout = 3;
  std::vector<std::string> temp = utils::string_split(line, " ");
  if (temp[0] == "GET")
    met = Http::GET;
  if (temp[0] == "POST")
    met = Http::POST;
  if (temp[0] == "DELETE")
    met = Http::DELETE;
  path = temp[1];
  err = parse_cgi(fd, temp[2]);
}

std::string RouteRule_CGI::parse_cgi(FileDescriptor &fd, std::string line) {
  std::string err_msg = "";
  std::string file_line = utils::remove_char(line, '$');

  err_msg = RouteRule_CGI::parse_executable(file_line, this->executable, this->env);
  if (err_msg != "")
    return err_msg;
  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      return "";
    file_line = utils::remove_char(temp.value(), '\n');
    if (utils::match_indent_level(file_line, 2) == false ||
        (file_line.empty() || file_line[file_line.length() - 1] == ' ' ||
         file_line[file_line.length() - 1] == '\t'))
      return "Error: \"" + file_line + "\" Indentation or space error";
    file_line = utils::trim_whitespace(file_line);
    if (is_timeout(file_line))
      timeout = parse_timeout(file_line);
    else if (std::string::npos != file_line.find("="))
      err_msg = RouteRule_CGI::parse_env(file_line, this->env);
    else
      err_msg = "Error: \"" + file_line + "\" Syntax error";
    if (err_msg != "")
      return err_msg;
  }
}

bool RouteRule_CGI::is_executable_file(const std::string &path) {
  // struct stat st;

  // if (stat(path.c_str(), &st) != 0)
  //   return false;

  // if (!S_ISREG(st.st_mode))
  //   return false;

  // return access(path.c_str(), X_OK) == 0;
  (void)path;
  return true;
}

bool RouteRule_CGI::is_cgi(const std::string &line) {
  std::size_t i = 1;

  if (line.empty() || line[0] != '$' || utils::has_space(line))
    return false;

  std::size_t pos = line.find(".cgi");
  if (std::string::npos != pos) {
    i = pos + 4;
    if (!RouteRule_CGI::is_executable_file(line.substr(1, pos + 3)))
      return false;
  } else {
    while (i < line.length() && line[i] != '(') {
      if (std::isdigit(line[i]))
        i++;
      else
        return false;
    }
  }
  if (i < line.length() && line[i] == '(') {
    std::size_t end = line.find(')');
    if (std::string::npos == end)
      return false;
    if (end + 1 != line.length())
      return false;
  }
  return true;
}

bool RouteRule_CGI::is_timeout(const std::string &line) {
  if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
    return false;
  double data = 0;
  bool dot = false;
  for (size_t i = 3; i < line.length(); ++i) {
    if (line[i] == '.') {
      if (dot)
        return false;
      dot = true;
    } else if (!std::isdigit(static_cast<unsigned char>(line[i])))
      return false;
  }
  std::stringstream ss(line.substr(3));
  if (!(ss >> data))
    return false;
  if (data > 15.0 || 0.05 >= data)
    return false;
  return true;
}

double RouteRule_CGI::parse_timeout(std::string &line) {
  double time = 3;
  std::stringstream oss;
  oss << line.erase(0, 3);
  oss >> time;
  return time;
}

static bool is_key(const std::string &key) {
  std::size_t i = 0;

  if (key.empty())
    return false;

  while (i < key.length()) {
    if (std::isupper(key[i]) || key[i] == '_' ||
        (i != 0 && std::isdigit(static_cast<unsigned char>(key[i]))))
      i++;
    else
      return false;
  }
  return true;
}

std::string RouteRule_CGI::parse_env(const std::string &line,
                      std::map<std::string, std::string> &env) {
  std::vector<std::string> key_and_value = utils::string_split(line, "=");
  if (key_and_value.size() != 2)
    return "Error: \"" + line + "\" Invalid environment variable syntax";
  if (!is_key(key_and_value[0]))
    return "Error: \"" + key_and_value[0] +
           "\" Invalid environment variable value";
  if (env.find(key_and_value[0]) != env.end())
    return "Error: \"" + line + "\" duplicate key error";
  env[key_and_value[0]] = key_and_value[1];
  return "";
}

bool is_uwsgi(std::vector<std::string> data) {
  if (data.size() != 2)
    return false;
  if (!RouteRule_CGI::is_executable_file(data[0]))
    return false;
  for (std::size_t i = 0; i < data[1].size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(data[1][i])))
      return false;
  }
  return true;
}

std::string RouteRule_CGI::parse_config_uwsgi(FileDescriptor &fd,
                               std::map<std::string, std::string> &uwsgi) {
  std::vector<std::string> value_and_key;
  std::string line = "";

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      return "";
    line = utils::remove_char(temp.value(), '\n');
    if (utils::match_indent_level(line, 1) == false ||
        (line.empty() || line[line.length() - 1] == ' ' ||
         line[line.length() - 1] == '\t'))
      return "Error: \"" + line + "\" Indentation or space error";
    line = utils::trim_whitespace(line);
    value_and_key = utils::string_split(line, ":");
    if (!is_uwsgi(value_and_key))
      return "Error: \"" + line + "\" uwsgi syntax error";
    if (value_and_key[0].find(".py") == std::string::npos)
      return "Error: \"" + line + "\" It is not a .py file";
    if (uwsgi.find(value_and_key[1]) != uwsgi.end())
      return "Error: \"" + line + "\" uwsgi syntax error";
    else {
      uwsgi[value_and_key[1]] = value_and_key[0];
    }
  }
  return "";
}

bool RouteRule_CGI::is_config_cgi(std::string line) {
  std::vector<std::string> split_line = utils::string_split(line, " ");
  if (split_line.size() != 3)
    return false;
  if (split_line[0] != "POST" && split_line[0] != "GET" &&
      split_line[0] != "DELETE")
    return false;
  if (utils::has_space(split_line[1]))
    return false;
  return true;
}

std::ostream &operator<<(std::ostream &os, const RouteRule_CGI &data) {
  std::map<std::string, std::string> env = data.get_env();
  std::map<std::string, std::string>::const_iterator env_it;

  os << "\nExecutable: " << data.get_executable() << "\n";
  os << "\n\tEnv\n";
  for (env_it = env.begin(); env_it != env.end(); ++env_it) {
    os << "\tEnv key: " << env_it->first << ", Env value: " << env_it->second
       << "\n";
  }
  os << "\n\tTimeout: " << data.get_timeout() << "\n";

  return (os);
}

std::string RouteRule_CGI::parse_executable(const std::string &line, std::string &executable,
                             std::map<std::string, std::string> &map) {
  std::string err_msg = "";

  std::string file_line = utils::remove_char(line, '$');
  std::size_t start = file_line.find('(');
  if (std::string::npos != start) {
    std::size_t end = file_line.find(')');
    executable = file_line.substr(0, start);
    std::string env = file_line.substr(start + 1, end - start - 1);
    err_msg = RouteRule_CGI::parse_env(env, map);
    if (err_msg != "")
      return err_msg;
  } else
    executable = file_line;
  return err_msg;
}
