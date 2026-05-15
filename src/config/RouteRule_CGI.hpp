#ifndef ROUTERULE_CGI_HPP
#define ROUTERULE_CGI_HPP

#include "PathPattern.hpp"

class RouteRule_CGI {
private:
  Request::Method met;
  PathPattern path;
  std::string executable;
  std::map<std::string, std::string> env;
  unsigned int timeout_ms;
  std::string err_meg;
  std::size_t count_line;
  std::vector<std::string> file_extension;

  static bool is_valid_timeout(const std::string &line);
  std::string parse_timeout_value(std::string &line);
  std::string parse_cgi_block(FileDescriptor &fd, std::string line);
  std::string parse_env_entry(const std::string &line,
                                     std::map<std::string, std::string> &env);
  std::string parse_executable(const std::string &line,
                                      std::string &executable,
                                      std::map<std::string, std::string> &env);

  std::string matches_route_cgi_syntax(const std::string &line);
  std::string parse_cgi_params(FileDescriptor &fd);
public:
  RouteRule_CGI() : met(Request::GET), path(""), executable(""), env(), timeout_ms(3000), err_meg(""), count_line(0) {};
  RouteRule_CGI(FileDescriptor &fd, const std::string &line, const std::vector<std::string> &file_extension);
  RouteRule_CGI &operator=(const RouteRule_CGI &other) {
    if (this != &other) {
      met = other.met;
      path = other.path;
      executable = other.executable;
      env = other.env;
      timeout_ms = other.timeout_ms;
      err_meg = other.err_meg;
      count_line = other.count_line;
      file_extension = other.file_extension;
    }
    return *this;
  }
  const PathPattern get_path(void) const { return path; }
  Request::Method get_method(void) const { return met; }
  const std::string get_err_meg(void) const { return err_meg; }
  const std::string get_executable(void) const { return executable; }
  const std::map<std::string, std::string> get_env(void) const { return env; }
  std::size_t get_count_line(void) const { return count_line; }
  unsigned int get_timeout_ms() const { return timeout_ms; }

  static bool is_valid_env_key(const std::string &key);
  static bool is_valid_cgi_config(std::string line);
  static std::string is_executable_file(const std::string &path);
  static std::string
  parse_global_cgi_block(FileDescriptor &fd,
                    std::map<std::string, std::string> &global_cgi, std::size_t &count_line);
};

std::ostream &operator<<(std::ostream &os, const RouteRule_CGI &data);

#endif