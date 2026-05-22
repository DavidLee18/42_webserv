#include "RouteRule_CGI.hpp"

RouteRule_CGI::RouteRule_CGI(FileDescriptor& fd, const std::string& line,
                             const std::vector<std::string>& file_extension,
                             char**                          envp) {
    err_meg                       = "";
    timeout_ms                    = 3000;
    count_line                    = 0;
    origin_line                   = "\t" + line;
    this->file_extension          = file_extension;

    std::vector<std::string> temp = utils::string_split(line, " ");

    if (temp[0] == "GET")
        met = Request::GET;
    else if (temp[0] == "POST")
        met = Request::POST;
    else if (temp[0] == "DELETE")
        met = Request::DELETE;
    path                = PathPattern(temp[1]);

    Result<Void> result = parse_cgi_block(fd, line, envp);
    if (!result.error().empty()) err_meg = result.error();
}

Result<Void> RouteRule_CGI::parse_cgi_block(FileDescriptor& fd,
                                            std::string line, char** envp) {
    std::vector<std::string> split = utils::string_split(line, " ");
    if (split[2][0] != '$')
        return ERR(Void,
                   ConfigError::make(origin_line, split[2],
                                     ERR_CGI_SCRIPT_MISSING_DOLLAR_PREFIX));
    std::string file_line = utils::remove_char(split[2], '$');

    TRY_(Void, Void,
         parse_routerule_cgi_executable(file_line, this->executable, this->env,
                                        envp))
    TRY_(Void, Void, parse_cgi_params(fd))

    return OKV;
}

Result<Void> RouteRule_CGI::parse_cgi_params(FileDescriptor& fd) {
    std::string file_line = "";

  while (true) {
    count_line++;
    TRY(Void, std::string, file_line, fd.read_file_line())
    if (file_line == "\n" || file_line == "")
      return OKV;
    TRY_(Void, Void, configutils::prepare_config_line(origin_line, file_line, 2))

        if (utils::has_space(file_line))
            return ERR(Void, ConfigError::make(origin_line, file_line,
                                               ERR_INVALID_CGI_EXTENSION_LINE));
        else if (is_valid_timeout(file_line)) {
            file_line = file_line.substr(3);
            err_meg =
                configutils::string_to_unsigned_int(file_line, timeout_ms);
            if (err_meg != "")
                return ERR(Void,
                           ConfigError::make(origin_line, file_line, err_meg));
            else if (timeout_ms > CGI_MAX_TIMEOUT ||
                     CGI_MIN_TIMEOUT > timeout_ms)
                return ERR(Void,
                           ConfigError::make(origin_line, file_line,
                                             ERR_INVALID_CGI_TIMEOUT_RANGE));
        } else if (std::string::npos != file_line.find("=")) {
            TRY_(Void, Void, parse_env_entry(file_line, env))
        } else
            return ERR(Void, ConfigError::make(origin_line, file_line,
                                               ERR_INVALID_CGI_EXTENSION_LINE));
    }
    return OKV;
}

Result<Void> RouteRule_CGI::is_executable_file(const std::string& origin_line,
                                               const std::string& path,
                                               bool   allow_absolute_path,
                                               char** envp) {
    std::string real_path = utils::get_env("PWD", envp) + path;
    if (allow_absolute_path == true) real_path = path;
    struct stat st;
    if (stat(real_path.c_str(), &st) != 0)
        return ERR(Void,
                   ConfigError::make(origin_line, path, ERR_INVALID_FILE_PATH));

    if (!S_ISREG(st.st_mode))
        return ERR(Void,
                   ConfigError::make(origin_line, path, ERR_NOT_REGULAR_FILE));

    bool is_cgi_file = real_path.length() >= 4 &&
                       real_path.rfind(".cgi") == real_path.length() - 4;
    if (!is_cgi_file) {
        if (access(real_path.c_str(), R_OK) != 0)
            return ERR(Void, ConfigError::make(origin_line, path,
                                               ERR_FILE_NOT_READABLE));
    } else {
        if (access(real_path.c_str(), X_OK) != 0)
            return ERR(Void, ConfigError::make(origin_line, path,
                                               ERR_FILE_NOT_EXECUTABLE));
    }
    return OKV;
}

Result<Void>
RouteRule_CGI::matches_routerule_cgi_syntax(const std::string& line,
                                            char**             envp) {
    std::size_t pos      = line.find('(');
    std::size_t i        = 0;
    std::size_t exec_end = 0;
    std::string exec_path =
        (pos == std::string::npos) ? line : line.substr(0, pos);

    for (std::size_t index = 0; index < file_extension.size(); ++index) {
        pos = exec_path.rfind(file_extension[index]);
        if (pos != std::string::npos &&
            pos == exec_path.length() - file_extension[index].size()) {
            exec_end = pos + file_extension[index].length();
            break;
        }
    }
    if (exec_end == 0)
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_UNDEFINED_GLOBAL_CGI_EXTENSION));
    else if (exec_end < line.length() && line[exec_end] != '(')
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_INVALID_CGI_INLINE_ENV_SYNTAX));

    exec_path = line.substr(0, exec_end);
    TRY_(Void, Void,
         RouteRule_CGI::is_executable_file(origin_line, exec_path, false, envp))

    i = exec_end;
    if (i == line.length()) return OKV;
    if (line[i] != '(')
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_INVALID_CGI_INLINE_ENV_SYNTAX));

    std::size_t equals = line.find('=', i + 1);
    std::size_t end    = line.find(')', i + 1);

    if (equals == std::string::npos)
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_INVALID_CGI_ENV_MISSING_EQUAL));
    else if (end == std::string::npos)
        return ERR(Void,
                   ConfigError::make(origin_line, line,
                                     ERR_INVALID_CGI_ENV_MISSING_CLOSE_PAREN));
    else if (end + 1 != line.length())
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_INVALID_CGI_ENV_TRAILING_CHARS));
    else if (line.find('=', equals + 1) != std::string::npos)
        return ERR(
            Void, ConfigError::make(origin_line, line,
                                    ERR_INVALID_CGI_ENV_MULTIPLE_DECLARATIONS));
    else if (equals <= i + 1 || equals + 1 >= end)
        return ERR(Void,
                   ConfigError::make(origin_line, line,
                                     ERR_INVALID_CGI_ENV_EMPTY_KEY_OR_VALUE));
    return OKV;
}

bool RouteRule_CGI::is_valid_timeout(const std::string& line) {
    if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
        return false;
    return true;
}

bool RouteRule_CGI::is_valid_env_key(const std::string& key) {
    std::size_t i = 0;

    if (key.empty()) return false;

    while (i < key.length()) {
        if (std::isupper(key[i]) || key[i] == '_' ||
            (i != 0 && std::isdigit(static_cast<unsigned char>(key[i]))))
            i++;
        else
            return false;
    }
    return true;
}

Result<Void>
RouteRule_CGI::parse_env_entry(const std::string&                  line,
                               std::map<std::string, std::string>& env) {
    std::vector<std::string> key_and_value = utils::string_split(line, "=");
    std::size_t              pos           = line.find("=");

    if (pos == std::string::npos)
        return ERR(Void, ConfigError::make(origin_line, line,
                                           ERR_INVALID_CGI_ENV_FORMAT));
    else if (utils::count_occurrences(line, "=") != 1) {
        pos = line.find("=", pos + 1);
        return ERR(Void, ConfigError::make(origin_line, line.substr(pos),
                                           ERR_INVALID_CGI_ENV_FORMAT));
    } else if (key_and_value.size() != 2)
        return ERR(Void,
                   ConfigError::make(origin_line, line.substr(line.find("=")),
                                     ERR_INVALID_CGI_ENV_KEY_VALUE));
    else if (!RouteRule_CGI::is_valid_env_key(key_and_value[0]))
        return ERR(Void, ConfigError::make(origin_line, key_and_value[0],
                                           ERR_INVALID_CGI_ENV_KEY));
    else if (env.find(key_and_value[0]) != env.end())
        return ERR(Void, ConfigError::make(origin_line, key_and_value[0],
                                           ERR_DUPLICATE_CGI_ENV_KEY));
    env[key_and_value[0]] = key_and_value[1];
    return OKV;
}

Result<Void> RouteRule_CGI::parse_global_cgi_block(
    FileDescriptor& fd, std::map<std::string, std::string>& global_cgi,
    std::size_t& count_line, char** envp) {
    std::string line        = "";
    std::string err         = "";
    std::string origin_line = "";

  while (true) {
    count_line++;
    TRY(Void, std::string, line, fd.read_file_line())
    if (line == "\n" || line == "")
      break;
    TRY_(Void, Void, configutils::prepare_config_line(origin_line, line, 1))

        std::size_t pos = line.find("->");
        if (pos == std::string::npos)
            return ERR(Void, ConfigError::make(
                                 origin_line, line,
                                 ERR_MISSING_GLOBAL_CGI_MAPPING_OPERATOR));
        if (utils::count_occurrences(line, "->") != 1) {
            pos = line.find("->", pos + 1);
            return ERR(
                Void, ConfigError::make(origin_line, line.substr(pos),
                                        ERR_INVALID_GLOBAL_CGI_MAPPING_SYNTAX));
        }
        std::vector<std::string> key_and_value =
            utils::string_split(line, "->");
        if (key_and_value.size() != 2)
            return ERR(Void,
                       ConfigError::make(origin_line, line,
                                         ERR_INVALID_GLOBAL_CGI_MAPPING_VALUE));

        std::string key   = utils::trim_whitespace(key_and_value[0]);
        std::string value = utils::trim_whitespace(key_and_value[1]);
        if (utils::has_space(key))
            return ERR(Void, ConfigError::make(
                                 origin_line, key,
                                 ERR_INVALID_GLOBAL_CGI_EXTENSION_WHITESPACE));
        else if (utils::has_invalid_char(key, "-_"))
            return ERR(
                Void, ConfigError::make(origin_line, key,
                                        ERR_INVALID_GLOBAL_CGI_EXTENSION_CHAR));
        else if (utils::has_space(value))
            return ERR(Void,
                       ConfigError::make(
                           origin_line, value,
                           ERR_INVALID_GLOBAL_CGI_EXECUTABLE_PATH_WHITESPACE));
        else if (global_cgi.find(key) != global_cgi.end())
            return ERR(Void,
                       ConfigError::make(origin_line, key,
                                         ERR_DUPLICATE_GLOBAL_CGI_MAPPING));
        TRY_(Void, Void, is_executable_file(origin_line, value, true, envp))
        global_cgi[key] = value;
    }
    return OKV;
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

Result<Void> RouteRule_CGI::parse_routerule_cgi_executable(
    const std::string& line, std::string& executable,
    std::map<std::string, std::string>& map, char** envp) {
    std::string err_msg   = "";
    std::string file_line = utils::remove_char(line, '$');

    TRY_(Void, Void, matches_routerule_cgi_syntax(file_line, envp))

    std::size_t start = file_line.find('(');

    if (std::string::npos != start) {
        std::size_t end = file_line.find(')');
        executable      = file_line.substr(0, start);

        TRY_(Void, Void,
             is_executable_file(origin_line, executable, false, envp))

        std::string env = file_line.substr(start + 1, end - start - 1);
        TRY_(Void, Void, parse_env_entry(env, map))
    } else {
        executable = file_line;
        TRY_(Void, Void,
             is_executable_file(origin_line, executable, false, envp))
    }
    return OKV;
}

std::ostream& operator<<(std::ostream& os, const RouteRule_CGI& data) {
    std::map<std::string, std::string>                 env = data.get_env();
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
