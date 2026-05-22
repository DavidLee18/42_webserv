#ifndef ROUTERULE_CGI_HPP
#define ROUTERULE_CGI_HPP

#include "../config/PathPattern.hpp"

#define CGI_MAX_TIMEOUT 3600000
#define CGI_MIN_TIMEOUT 1

class RouteRule_CGI {
private:
  // Final parsed values for this CGI route rule.
  Request::Method met;
  PathPattern path;
  std::string executable;
  std::map<std::string, std::string> env;
  unsigned int timeout_ms;
  // Temporary state used only while parsing and validating this CGI route rule.
  std::string err_meg;
  std::string origin_line;
  std::size_t count_line;
  std::vector<std::string> file_extension;

    static bool              is_valid_timeout(const std::string& line);
    Result<Void> parse_cgi_block(FileDescriptor& fd, std::string line,
                                 char** envp);
    Result<Void> parse_env_entry(const std::string&                  line,
                                 std::map<std::string, std::string>& env);
    Result<Void> parse_routerule_cgi_executable(
        const std::string& line, std::string& executable,
        std::map<std::string, std::string>& env, char** envp);

    Result<Void>        matches_routerule_cgi_syntax(const std::string& line,
                                                     char**             envp);
    Result<Void>        parse_cgi_params(FileDescriptor& fd);
    static Result<Void> is_executable_file(const std::string& origin_line,
                                           const std::string& path,
                                           bool   allow_absolute_path,
                                           char** envp);

  public:
    RouteRule_CGI()
        : met(Request::ERROR), path(), executable(), env(), timeout_ms(3000),
          err_meg(), origin_line(), count_line(0), file_extension(){};
    RouteRule_CGI(FileDescriptor& fd, const std::string& line,
                  const std::vector<std::string>& file_extension, char** envp);
    RouteRule_CGI& operator=(const RouteRule_CGI& other) {
        if (this != &other) {
            this->met            = other.met;
            this->path           = other.path;
            this->executable     = other.executable;
            this->env            = other.env;
            this->timeout_ms     = other.timeout_ms;
            this->err_meg        = other.err_meg;
            this->origin_line    = other.origin_line;
            this->count_line     = other.count_line;
            this->file_extension = other.file_extension;
        }
        return *this;
    }
    const PathPattern&     get_path(void) const { return path; }
    const Request::Method& get_method(void) const { return met; }
    const std::string&     get_err_meg(void) const { return err_meg; }
    const std::string&     get_executable(void) const { return executable; }
    const std::map<std::string, std::string>& get_env(void) const {
        return env;
    }
    const std::size_t&  get_count_line(void) const { return count_line; }
    const unsigned int& get_timeout_ms() const { return timeout_ms; }

    static bool         is_valid_env_key(const std::string& key);
    static bool         is_valid_cgi_config(std::string line);
    static Result<Void>
    parse_global_cgi_block(FileDescriptor&                     fd,
                           std::map<std::string, std::string>& global_cgi,
                           std::size_t& count_line, char** envp);
};

std::ostream& operator<<(std::ostream& os, const RouteRule_CGI& data);

#endif
