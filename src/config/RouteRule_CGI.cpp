#include "RouteRule_CGI.hpp"

RouteRule_CGI::RouteRule_CGI(FileDescriptor &fd, const std::string &line) {
  err_meg = "";
  timeout_ms = 3000;
  count_line = 0;
  worker_instance = 0;

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
    return "on [" + line + "], [" + split[2] + "]: The CGI script reference does not start with the required $ prefix. (CGI syntax rule, the script identifier must begin with $ to be recognized as a valid CGI command, but the provided value does not follow this required format).";
  std::string file_line = utils::remove_char(split[2], '$');

  err_meg = RouteRule_CGI::parse_executable(file_line, this->executable, this->env);
  if (err_meg != "")
    return "on [\t" + line + err_meg;
  err_meg = RouteRule_CGI::parse_cgi_params(*this, fd, "");
  if (err_meg != "")
    return err_meg;
  return "";
}

std::string RouteRule_CGI::parse_cgi_params(RouteRule_CGI& cgi, FileDescriptor &fd, std::string executable) {
  std::string file_line = "";
  std::string err_format = "on [\t\t";
  bool is_server = true;
  bool is_server_uwsgi = false;
  if (executable != "") {
    is_server = false;
    err_format = "on [\t";
    cgi.executable = executable;
  } else if (std::isdigit(static_cast<unsigned char>(cgi.executable[0])))
    is_server_uwsgi = true;

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    cgi.count_line++;
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      return  "";
    else if (is_server_uwsgi)
      return err_format + utils::remove_char(temp.value(), '\n') + "], [" + utils::remove_char(temp.value(), '\n') + "]:Invalid RouteRule (uWSGI mode does not support additional information in RouteRule)";
    
    file_line = utils::remove_char(temp.value(), '\n');
    cgi.err_meg = utils::get_indent_whitespace_error(file_line, 2);
    if (cgi.err_meg != "")
      return cgi.err_meg;
    file_line = utils::trim_whitespace(file_line);

    if (utils::has_space(file_line))
      return err_format + file_line + "], [" + file_line + "]: The CGI extended information line does not match any of the allowed formats. (CGI extension rule, the line must follow either key=value or ...<numeric string> format, but the provided line does not conform to either pattern).";
    else if (is_valid_timeout(file_line)) {
      cgi.err_meg = parse_timeout_value(cgi, file_line);
      if (cgi.err_meg != "")
        return cgi.err_meg;
    }
    else if (std::string::npos != file_line.find("=")) {
      cgi.err_meg = RouteRule_CGI::parse_env_entry(file_line, cgi.env);
      if (cgi.err_meg != "")
        return err_format + file_line + cgi.err_meg;
    } else if (file_line[0] == '*') {
      if (cgi.worker_instance == 0 || is_server)
        return err_format + file_line + "], [" + file_line + "]:Invalid worker_instance (this parameter can only be configured in the server-side global uWSGI configuration and is not allowed in CGI or server-side uWSGI additional parameters).";
      char* end;
      std::string worker_instance_data = file_line.substr(1);
      unsigned long num = std::strtoul(worker_instance_data.c_str(), &end, 10);

      if (*end != '\0')
        return err_format + file_line + "], [" + worker_instance_data + "]:Invalid worker_instance (must contain only digits (0-9), but non-numeric characters were found).";
      else if (num > 4096 || num < 1)
        return err_format + file_line + "], [" + worker_instance_data + "]: Invalid worker_instance (must be within the range 1 ~ 4096, but the provided value is outside this range).";
      else
        cgi.worker_instance = static_cast<int>(num);
    } else
        return err_format + file_line + "], [" + file_line + "]: The CGI extended information line does not match any of the allowed formats. (CGI extension rule, the line must follow either key=value or ...<numeric string> format, but the provided line does not conform to either pattern).";
  }
}

std::string RouteRule_CGI::is_executable_file(const std::string &path) {
  // char cwd[4096];
  // getcwd(cwd, sizeof(cwd));

  // std::string real_path = std::string(cwd) + path;
  // struct stat st;

  // if (stat(real_path.c_str(), &st) != 0)
  //   return "Violates file existence rule (the specified path does not exist or cannot be accessed).";

  // if (!S_ISREG(st.st_mode))
  //   return "Violates regular file rule (the given path is not a regular file).";

  // if (access(path.c_str(), X_OK) != 0)
  //   return "Violates executable permission rule (the file does not have execute permission).";
  (void)path;
  return "";
}

std::string RouteRule_CGI::matches_cgi_syntax(const std::string &line) {
  std::size_t i = 0;
  std::size_t pos = line.find(".cgi", i);
  if (pos != std::string::npos) {
    std::size_t exec_end = pos + 4;

    if (exec_end < line.length() && line[exec_end] != '(')
      return "Invalid CGI environment variable syntax (violates the environment variable format rule, additional environment variables after the .cgi extension must start with '(' in the form '(key=value)').";
    std::string exec_path = line.substr(0, exec_end);
    if (RouteRule_CGI::is_executable_file(exec_path) != "")
      return RouteRule_CGI::is_executable_file(exec_path);

    i = exec_end;
  } else {
    while (i < line.length() && line[i] != '(') {
      if (!std::isdigit(static_cast<unsigned char>(line[i])))
        return "Invalid uWSGI port format (violates the uWSGI port rule: the port number must be a numeric value).";
      ++i;
    }
    if (line.size() != i) 
      return "Invalid uWSGI environment variable syntax (violates the uWSGI configuration rule: environment variables must be defined in the global uWSGI configuration, not inline).";
  }

  if (i == line.length())
    return "";
  if (line[i] != '(')
    return "Invalid CGI environment variable syntax (violates the environment variable format rule: additional environment variables after the .cgi extension must start with '(' in the form '(key=value)').";

  std::size_t equals = line.find('=', i + 1);
  std::size_t end = line.find(')', i + 1);

  if (line.find('=', equals + 1) != std::string::npos)
    return "Invalid environment variable syntax (violates the environment variable rule: multiple environment variable declarations are not permitted; only a single '(key=value)' is allowed).";
  else if (equals == std::string::npos)
    return "Invalid environment variable syntax (violates the environment variable format rule: missing '=' in '(key=value)' declaration).";
  else if(end == std::string::npos)
    return "Invalid environment variable syntax (violates the environment variable format rule: missing closing ')' in '(key=value)' declaration).";
  else if (equals <= i + 1 || equals + 1 >= end)
    return "Invalid environment variable syntax (violates the key-value format rule: missing key or value in '(key=value)' declaration).";
  else if (end + 1 != line.length())
    return "Invalid environment variable syntax (violates the environment variable format rule: trailing characters found after the closing ')' in '(key=value)' declaration).";
  return "";
}

bool RouteRule_CGI::is_valid_timeout(const std::string &line) {
  if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
    return false;
  return true;
}

std::string RouteRule_CGI::parse_timeout_value(RouteRule_CGI &cgi, std::string &line) {
  int data = 0;
  for (size_t i = 3; i < line.length(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(line[i])))
      return "on [\t" + line + "], [" + &line[3] + "]: The CGI response time configuration contains an invalid value type after the delimiter. (CGI response time rule, the value after ... must consist only of numeric characters, but non-numeric characters are present, making it invalid for parsing).";
  }
  std::stringstream ss(line.substr(3));
  ss >> data;

  if (data > 3600000 || 1 > data)
    return "on [\t" + line + "], [" + &line[3] + "]: The CGI response time configuration is out of the allowed range. (CGI response time rule, the value must be between 1ms and 3600000ms inclusive, but the provided value falls outside this range).";
  cgi.timeout_ms = data;
  return "";
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
  std::string res = "], [";
  if (utils::count_occurrences(line, "=") != 1) {
    std::size_t pos = line.find("=");
    pos = line.find("=", pos); 
    return  res + &line[pos] + "]: The CGI environment variable assignment contains multiple = characters. (environment variable rule, each assignment must follow a single key=value format, but multiple = symbols are present, making the format invalid).";
  } else if (key_and_value.size() != 2)
    return res + &line[line.find("=")] + "]: The CGI environment variable assignment contains an invalid key-value format. (environment variable rule, each assignment must follow key=value, but either the key or value is missing, making the format invalid)";
  else if (!RouteRule_CGI::is_valid_env_key(key_and_value[0]))
    return res + key_and_value[0] + "]: The CGI extended information key contains invalid characters or format. (CGI extension rule, the key must consist of uppercase letters, underscores, and digits not allowed at the first position, but the provided key violates these constraints, making it invalid).";
  else if (env.find(key_and_value[0]) != env.end())
    return res + key_and_value[0] + "]: The CGI extended information contains a duplicate key definition. (CGI extension rule, each key in a key=value pair must be unique within the same request context, but the same key appears more than once, causing a conflict in value assignment).";
  env[key_and_value[0]] = key_and_value[1];
  return "";
}

std::string RouteRule_CGI::is_valid_uwsgi_config(std::vector<std::string> data) {
  if (data.size() != 2)
    return ": Invalid format(expected \"file_path:port\". The value must follow the required pattern with a Python file path and a numeric port separated by a colon.).";
  else if (RouteRule_CGI::is_executable_file(data[0]) != "")
    return  ", [" + data[0] + "]: " + RouteRule_CGI::is_executable_file(data[0]);
  
  for (std::size_t i = 0; i < data[1].size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(data[1][i])))
    return ", [" + data[1] + "]: " + "Violates port numeric rule (the port must consist only of digits).";
  }

  char* end;
  unsigned long port = std::strtoul(data[1].c_str(), &end, 10);
  if (port > 65535)
    return ", [" + data[1] + "]: " + "Violates port range rule (port must be between 0 and 65535).";
  return "";
}

std::string
RouteRule_CGI::parse_uwsgi_block(FileDescriptor &fd,
                                 std::map<int, RouteRule_CGI> &uwsgi, std::size_t &count_line) {
  std::vector<std::string> value_and_key;
  std::string line = "";
  std::string err = "";

  while (true) {
    Result<std::string> temp = fd.read_file_line();
    count_line++;
    if (temp.error() != "")
      return "FileDescriptor Error: " + temp.error();
    else if (temp.value() == "\n" || temp.value() == "")
      return "";
    
    line = utils::remove_char(temp.value(), '\n');
    err = utils::get_indent_whitespace_error(line, 1);
    if (err != "")
      return err;
    line = utils::trim_whitespace(line);

    std::vector<std::string> split = utils::string_split(line, " ");
    if (split.size() != 1) {
      for (std::size_t i = 1; i < split.size(); ++i)
        err += " " + split[i]; 
      return "on [\t" + line + "], [" + err + "]: Violates uwsgi entry rule (each line must contain exactly one configuration entry).";
    } else if (utils::count_occurrences(line, ":") != 1) {
      std::size_t pos = line.find(':');
      pos = line.find(':', pos + 1);
      return "on [\t" + line + "], [" + &line[pos] + "]: Violates uwsgi format rule (expected \"<file>:<port>\" without trailing ':' or extra delimiters).";
    }

    value_and_key = utils::string_split(line, ":");
    err = RouteRule_CGI::is_valid_uwsgi_config(value_and_key);
    if (err != "")
      return  "on [\t" + line + "]" + err;
    else if (value_and_key[0].length() < 3 ||
        value_and_key[0].substr(value_and_key[0].length() - 3) != ".py") {
      return "on [\t" + line + "], [" + value_and_key[0] + "]: Violates python file extension rule (the file must have a .py extension).";
      }
    char* end;
    unsigned long num = std::strtoul(value_and_key[1].c_str(), &end, 10);

    if (uwsgi.find(static_cast<int>(num)) != uwsgi.end())
      return "on [\t" + line + "], [" + value_and_key[1] + "]: Violates duplicate port rule (the port is already assigned to another file).";
    else if (*end != '\0')
      return "[" + line + "], [" + value_and_key[1] + "]: The port value must contain only numeric characters (the port value syntax rule is violated because the provided value contains non-numeric characters or cannot be fully converted to a number).";
    else if (num > 65535)
      return "[" + line + "], [" + value_and_key[1] + "]: Violates port range rule (port must be between 0 and 65535).";
  
    err = RouteRule_CGI::parse_cgi_params(uwsgi[static_cast<int>(num)], fd, value_and_key[0]);
    count_line += uwsgi[static_cast<int>(num)].get_count_line(); 
    if (err != "")
      return err;
  }
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

  if (data.get_worker_instance() == 0) {
    os << "\nCGI: ";
    if (data.get_method() == Request::GET)
      os << "GET";
    else if (data.get_method() == Request::POST)
      os << "POST";
    else if (data.get_method() == Request::DELETE)
      os << "DELETE";
    os << " " << data.get_path().to_string() << "\n";
  }
  os << "\tExecutable: " << data.get_executable();
  os << "\n\tEnv";
  for (env_it = env.begin(); env_it != env.end(); ++env_it) {
    os << "\n\t\tEnv key: " << env_it->first << ", Env value: " << env_it->second;
  }
  os << "\n\tTimeout: " << data.get_timeout_ms() << "\n";
  if (data.get_worker_instance() != 0)
    os << "\tworker_instance: " << data.get_worker_instance() << "\n";

  return (os);
}

std::string
RouteRule_CGI::parse_executable(const std::string &line,
                                std::string &executable,
                                std::map<std::string, std::string> &map) {
  std::string err_msg = "";
  std::string file_line = utils::remove_char(line, '$');
  err_msg = RouteRule_CGI::matches_cgi_syntax(file_line);
  if (err_msg != "")
    return "], ["+ file_line + "]: " + err_msg;
  std::size_t start = file_line.find('(');
  if (std::string::npos != start) {
    std::size_t end = file_line.find(')');
    executable = file_line.substr(0, start);
    err_msg = is_executable_file(executable);
    if (err_msg != "")
      return "], ["+ file_line + "]: " + err_msg;
    std::string env = file_line.substr(start + 1, end - start - 1);
    err_msg = RouteRule_CGI::parse_env_entry(env, map);
    if (err_msg != "")
      return err_msg;
  } else {
    err_msg = is_executable_file(executable);
    if (err_msg != "")
      return "], ["+ file_line + "]: " +  err_msg;
    executable = file_line;
  }
  return err_msg;
}
