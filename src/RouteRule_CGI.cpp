#include "RouteRule_CGI.hpp"

// Forward declarations of internal helper functions used in parse_CGI
bool is_CGI(const std::string &line);
static bool is_timeout(const std::string &line);
static double parse_timeout(std::string &line);
std::string parse_Executable(const std::string& line, std::string &executable, std::map<std::string, std::string> &map);

RouteRule_CGI::RouteRule_CGI(FileDescriptor &fd, std::string line) {
  err = "";
  timeout = 3;
  std::vector<std::string> temp = string_split(line, " ");
  err = parse_CGI(fd, temp[2]);
}

std::string RouteRule_CGI::parse_CGI(FileDescriptor &fd, std::string line) {
  std::string err_msg = "";

  if (!is_CGI(line))
    return "Error: \"" + line + "\" CGI executable not found";
  std::string file_line = trim_char(line, '$');
  err_msg = parse_Executable(file_line, this->executable, this->env);
  if (err_msg != "")
    return err_msg;
  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      return "";
    file_line = trim_char(temp.value(), '\n');
    if (is_tab_or_space(file_line, 2) == false ||
        (file_line.empty() || file_line[file_line.length() - 1] == ' ' ||
         file_line[file_line.length() - 1] == '\t'))
      return "Error: \"" + file_line + "\" Indentation or space error";
    file_line = trim_space(file_line);
    if (is_timeout(file_line))
      timeout = parse_timeout(file_line);
    else if (std::string::npos != file_line.find("="))
      err_msg = parse_env(file_line, this->env);
    else
      err_msg = "Error: \"" + file_line + "\" Syntax error";
    if (err_msg != "")
      return err_msg;
  }
}

bool isExecutableFile(const std::string &path) {
  // struct stat st;

  // if (stat(path.c_str(), &st) != 0)
  //   return false;

  // if (!S_ISREG(st.st_mode))
  //   return false;

  // return access(path.c_str(), X_OK) == 0;
  (void)path;
  return true;
}

bool is_CGI(const std::string &line) {
  std::size_t i = 1;

  if (line.empty() || line[0] != '$' || is_have_space(line))
    return false;

  std::size_t pos = line.find(".cgi");
  if (std::string::npos != pos) {
    i = pos + 4;
    if (!isExecutableFile(line.substr(1, pos + 3)))
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

static bool is_timeout(const std::string &line) {
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

static double parse_timeout(std::string &line) {
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

std::string parse_env(const std::string &line, std::map<std::string, std::string> &env) {
  std::vector<std::string> key_and_value = string_split(line, "=");
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

static bool is_uwsgi(std::string line) { 
  (void)line;
  return true;
}

std::string parse_Config_uwsgi(FileDescriptor &fd,
                        std::map<std::string, std::string> &uwsgi) {
  std::vector<std::string> value_and_key;
  std::string line = "";

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      return "";
    line = trim_char(temp.value(), '\n');
    if (is_tab_or_space(line, 1) == false ||
        (line.empty() || line[line.length() - 1] == ' ' ||
         line[line.length() - 1] == '\t'))
      return "Error: \"" + line + "\" Indentation or space error";
    line = trim_space(line);
    if (!is_uwsgi(line))
      return "Error: \"" + line + "\" uwsgi syntax error";
    value_and_key = string_split(line, ":");
    if (uwsgi.find(value_and_key[1]) != uwsgi.end())
      return "Error: \"" + line + "\" uwsgi syntax error";
    else{
      uwsgi[value_and_key[1]] = value_and_key[0];
    }
  }
  return "";
}

bool is_Config_CGI(std::string line)
{
  (void)line;
  return true;
}

std::ostream &operator<<(std::ostream &os, const RouteRule_CGI &data)
{
  std::map<std::string, std::string> env = data.Get_env();
  std::map<std::string, std::string>::const_iterator env_it;

  os << "\nExecutable: " << data.Get_executable() << "\n";
  os << "\n\tEnv\n";
  for (env_it = env.begin(); env_it != env.end(); ++env_it) {
    os << "\tEnv key: " << env_it->first << ", Env value: " << env_it->second << "\n";
  }
  os << "\n\tTimeout: " << data.Get_timeout() << "\n";
  
  return (os);
}

std::string parse_Executable(const std::string& line, std::string &executable, std::map<std::string, std::string> &map)
{
  std::string err_msg = "";

  std::string file_line = trim_char(line, '$');
  std::size_t start = file_line.find('(');
  if (std::string::npos != start) {
    std::size_t end = file_line.find(')');
    executable = file_line.substr(0, start);
    std::string env = file_line.substr(start + 1, end - start - 1);
    err_msg = parse_env(env, map);
    if (err_msg != "")
      return err_msg;
  } else
    executable = file_line;
  return err_msg;
}
