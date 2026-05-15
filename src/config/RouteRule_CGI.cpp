#include "RouteRule_CGI.hpp"

RouteRule_CGI::RouteRule_CGI(FileDescriptor &fd, const std::string &line, const std::vector<std::string> &file_extension) {
  err_meg = "";
  timeout_ms = 3000;
  count_line = 0;

  this->file_extension = file_extension;
  std::vector<std::string> temp = utils::string_split(line, " ");

  if (temp[0] == "GET")
    met = Request::GET;
  else if (temp[0] == "POST")
    met = Request::POST;
  else if (temp[0] == "DELETE")
    met = Request::DELETE;
  path = PathPattern(temp[1]);
  err_meg = parse_cgi_block(fd, line);
}

std::string RouteRule_CGI::parse_cgi_block(FileDescriptor &fd,
                                           std::string line) {
  std::vector<std::string> split = utils::string_split(line, " ");
  if (split[2][0] != '$')
    return "on [" + line + "], [" + split[2] +
           "]: The CGI script reference does not start with the required $ "
           "prefix. (CGI syntax rule, the script identifier must begin with $ "
           "to be recognized as a valid CGI command, but the provided value "
           "does not follow this required format).";
  std::string file_line = utils::remove_char(split[2], '$');

  err_meg =
      RouteRule_CGI::parse_executable(file_line, this->executable, this->env);
  if (err_meg != "")
    return "on [\t" + line + err_meg;
  err_meg = RouteRule_CGI::parse_cgi_params(fd);
  if (err_meg != "")
    return err_meg;
  return "";
}

std::string RouteRule_CGI::parse_cgi_params(FileDescriptor &fd) {
  std::string file_line = "";
  std::string err_format = "on [\t\t";

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    count_line++;
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      return  "";

    file_line = utils::remove_char(temp.value(), '\n');
    err_meg = utils::get_indent_whitespace_error(file_line, 2);
    if (err_meg != "")
      return err_meg;
    file_line = utils::trim_whitespace(file_line);

    if (utils::has_space(file_line))
      return err_format + file_line + "], [" + file_line +
             "]: The CGI extended information line does not match any of the "
             "allowed formats. (CGI extension rule, the line must follow "
             "either key=value or ...<numeric string> format, but the provided "
             "line does not conform to either pattern).";
    else if (is_valid_timeout(file_line)) {
      err_meg = utils::string_to_unsigned_int(&file_line[3], timeout_ms);
      if (err_meg != "")
        return "on [\t" + file_line + "], [" + &file_line[3] + "]: " + err_meg;
      else if (timeout_ms > 3600000 || 1 > timeout_ms)
        return "on [\t" + file_line + "], [" + &file_line[3] + "]: The CGI response time configuration is out of the allowed range. (CGI response time rule, the value must be between 1ms and 3600000ms inclusive, but the provided value falls outside this range).";
    }
    else if (std::string::npos != file_line.find("=")) {
      err_meg = parse_env_entry(file_line, env);
      if (err_meg != "")
        return err_format + file_line + err_meg;
    } else
      return err_format + file_line + "], [" + file_line +
             "]: The CGI extended information line does not match any of the "
             "allowed formats. (CGI extension rule, the line must follow "
             "either key=value or ...<numeric string> format, but the provided "
             "line does not conform to either pattern).";
  }
}

std::string RouteRule_CGI::is_executable_file(const std::string &path) {
  char cwd[4096];
  getcwd(cwd, sizeof(cwd));

  std::string real_path = std::string(cwd) + path;
  struct stat st;
  if (stat(real_path.c_str(), &st) != 0)
    return "Violates file existence rule (the specified path does not exist or cannot be accessed).";

  if (!S_ISREG(st.st_mode))
    return "Violates regular file rule (the given path is not a regular file).";

  if (access(real_path.c_str(), X_OK) != 0)
    return "Violates executable permission rule (the file does not have execute permission).";
  return "";
}

std::string RouteRule_CGI::matches_route_cgi_syntax(const std::string &line) {
  std::size_t pos = line.find('(');
  std::size_t i = 0;
  std::size_t exec_end = 0;
  std::string exec_path =
    (pos == std::string::npos) ? line : line.substr(0, pos);

  for (std::size_t index = 0; index < file_extension.size(); ++index) {
    pos = exec_path.rfind(file_extension[index]);
    if (pos != std::string::npos && pos == exec_path.length() - file_extension[index].size()) {
        exec_end = pos + file_extension[index].length();
        break;
    }
  }
  if (exec_end == 0)
    return "Undefined file extension in global CGI mapping (the global CGI file extension rule is violated because the provided file extension is not defined in the allowed extension list).";
  else if (exec_end < line.length() && line[exec_end] != '(')
    return "Invalid CGI environment variable syntax (the CGI inline environment rule is violated because any characters after the executable path must start with '(' and follow the exact '(key=value)' format).";
  exec_path = line.substr(0, exec_end);
  if (RouteRule_CGI::is_executable_file(exec_path) != "")
    return RouteRule_CGI::is_executable_file(exec_path);

  i = exec_end;
  if (i == line.length())
    return "";
  if (line[i] != '(')
    return "Invalid CGI environment variable syntax (the CGI inline environment rule is violated because the token after the executable path is not '('; inline environment variables must use the exact '(key=value)' format).";

  std::size_t equals = line.find('=', i + 1);
  std::size_t end = line.find(')', i + 1);

  if (equals == std::string::npos)
    return "Invalid environment variable syntax (violates the environment "
           "variable format rule: missing '=' in '(key=value)' declaration).";
  else if (end == std::string::npos)
    return "Invalid environment variable syntax (violates the environment "
           "variable format rule: missing closing ')' in '(key=value)' "
           "declaration).";
  else if (end + 1 != line.length())
    return "Invalid environment variable syntax (violates the environment "
           "variable format rule: trailing characters found after the closing "
           "')' in '(key=value)' declaration).";
  else if (line.find('=', equals + 1) != std::string::npos)
    return "Invalid environment variable syntax (violates the environment "
           "variable rule: multiple environment variable declarations are not "
           "permitted; only a single '(key=value)' is allowed).";
  else if (equals <= i + 1 || equals + 1 >= end)
    return "Invalid environment variable syntax (violates the key-value format "
           "rule: missing key or value in '(key=value)' declaration).";

  return "";
}

bool RouteRule_CGI::is_valid_timeout(const std::string &line) {
  if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
    return false;
  return true;
}

bool RouteRule_CGI::is_valid_env_key(const std::string &key) {
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

std::string
RouteRule_CGI::parse_env_entry(const std::string &line,
                               std::map<std::string, std::string> &env) {
  std::vector<std::string> key_and_value = utils::string_split(line, "=");
  std::string prefix = "], [";
  if (utils::count_occurrences(line, "=") != 1) {
    std::size_t pos = line.find("=");
    pos = line.find("=", pos + 1); 
    return prefix + &line[pos] + "]: The CGI environment variable assignment contains multiple = characters. (environment variable rule, each assignment must follow a single key=value format, but multiple = symbols are present, making the format invalid).";
  } else if (key_and_value.size() != 2)
    return prefix + &line[line.find("=")] + "]: The CGI environment variable assignment contains an invalid key-value format. (environment variable rule, each assignment must follow key=value, but either the key or value is missing, making the format invalid)";
  else if (!RouteRule_CGI::is_valid_env_key(key_and_value[0]))
    return prefix + key_and_value[0] + "]: The CGI extended information key contains invalid characters or format. (CGI extension rule, the key must consist of uppercase letters, underscores, and digits not allowed at the first position, but the provided key violates these constraints, making it invalid).";
  else if (env.find(key_and_value[0]) != env.end())
    return prefix + key_and_value[0] + "]: The CGI extended information contains a duplicate key definition. (CGI extension rule, each key in a key=value pair must be unique within the same request context, but the same key appears more than once, causing a conflict in value assignment).";
  env[key_and_value[0]] = key_and_value[1];
  return "";
}

std::string
RouteRule_CGI::parse_global_cgi_block(FileDescriptor &fd,
                                 std::map<std::string, std::string> &global_cgi, std::size_t &count_line) {
  std::string line = "";
  std::string err = "";

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    count_line++;
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      break;

    line = utils::remove_char(temp.value(), '\n');
    err = utils::get_indent_whitespace_error(line, 1);
    if (err != "")
      return err;
    line = utils::trim_whitespace(line);

    if (utils::count_occurrences(line, "->") != 1)
      return "on [\t" + line +  "]: Invalid global CGI mapping syntax "
                                "(the global CGI mapping rule is violated because the mapping must "
                                "contain exactly one '->' operator in the form 'extension -> executable').";
    std::vector<std::string> key_and_value = utils::string_split(line, "->");
    if (key_and_value.size() != 2)
      return "on [\t" + line + "]: Invalid global CGI mapping value "
                                "(the global CGI mapping rule is violated because both the extension "
                                "and executable path are required in the form 'extension -> executable').";

    std::string key = utils::trim_whitespace(key_and_value[0]);
    std::string value = utils::trim_whitespace(key_and_value[1]);
    if (utils::has_space(key))
      return "on [\t" + line + "], [" + key + "]: Invalid file extension in global CGI mapping "
                                              "(the global CGI file extension rule is violated because the file "
                                              "extension contains whitespace).";
    else if (utils::has_invalid_char(key, "-_"))
      return "on [\t" + line + "], [" + key + "]: Invalid file extension in global CGI mapping (only alphanumeric characters, '-' and '_' are allowed; all other special characters, including '.', are not permitted).";
    else if (utils::has_space(value))
      return "on [\t" + line + "], [" + value + "]: Invalid executable path in global CGI mapping "
                                                "(the global CGI executable path rule is violated because the executable "
                                                "path contains whitespace).";
    else if (global_cgi.find(key) != global_cgi.end())
      return "on [\t" + line + "], [" + key + "]: Duplicate global CGI mapping definition (the duplicate global CGI mapping rule is violated because the same file extension is already assigned to another executable path).";
    else if (is_executable_file(value) != "")
      return "on [\t" + line + "], [" + value + "]: " + is_executable_file(value);

    if (err != "")
      return err;

    global_cgi[key] = value;
  }
  return err;
}

bool RouteRule_CGI::is_valid_cgi_config(std::string line) {
  std::vector<std::string> split_line = utils::string_split(line, " ");
  if (split_line.size() != 3)
    return false;
  else if (split_line[0] != "POST" && split_line[0] != "GET" &&
           split_line[0] != "DELETE")
    return false;
  else if (utils::has_space(split_line[1]))
    return false;
  return true;
}

std::ostream &operator<<(std::ostream &os, const RouteRule_CGI &data) {
  std::map<std::string, std::string> env = data.get_env();
  std::map<std::string, std::string>::const_iterator env_it;

  os << "\tExecutable: " << data.get_executable();
  os << "\n\tEnv";
  for (env_it = env.begin(); env_it != env.end(); ++env_it) {
    os << "\n"
       << utils::debug << "\t\tEnv key: " << env_it->first
       << ", Env value: " << env_it->second;
  }
  os << "\n\tTimeout(ms): " << data.get_timeout_ms() << "\n";

  return (os);
}

std::string
RouteRule_CGI::parse_executable(const std::string &line,
                                std::string &executable,
                                std::map<std::string, std::string> &map) {
  std::string err_msg = "";
  std::string file_line = utils::remove_char(line, '$');
  err_msg = matches_route_cgi_syntax(file_line);
  if (err_msg != "")
    return "], [" + file_line + "]: " + err_msg;
  std::size_t start = file_line.find('(');
  if (std::string::npos != start) {
    std::size_t end = file_line.find(')');
    executable = file_line.substr(0, start);
    err_msg = is_executable_file(executable);
    if (err_msg != "")
      return "], [" + file_line + "]: " + err_msg;
    std::string env = file_line.substr(start + 1, end - start - 1);
    err_msg = parse_env_entry(env, map);
    if (err_msg != "")
      return err_msg;
  } else {
    executable = file_line;
    err_msg = is_executable_file(executable);
    if (err_msg != "")
      return "], ["+ file_line + "]: " +  err_msg;
  }
  return err_msg;
}
