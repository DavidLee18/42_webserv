#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "file_descriptor.h"
#include "http_1_1.h"
#include <unistd.h>

typedef std::map<std::string, std::map<std::string, std::string> > Header;
enum RouteType { ROUTE_REDIRECT, ROUTE_STATIC, ROUTE_OTHER };

enum RuleOperator {
  MULTIPLECHOICES,   // 300
  REDIRECT,          // 301
  FOUND,             // 302
  SEEOTHER,          // 303
  NOTMODIFIED,       // 304
  TEMPORARYREDIRECT, // 307
  PERMANENTREDIRECT, // 308
  AUTOINDEX,         // <i-
  POINT,             // ->
  SERVEFROM,         // <-
  UNDEFINED,
};

class PathPattern {
private:
  std::vector<std::string> path;

  bool match(std::string wildcard, std::string path) const;

public:
  PathPattern() : path() {}
  explicit PathPattern(const std::string &pathStr) : path(string_split(pathStr, "/")) {}
  PathPattern(std::vector<std::string> path) : path(path) {}

  bool operator==(const std::string &) const;
  friend bool operator==(const std::string &line, const PathPattern &path) {
    return (path == line);
  }
  const std::vector<std::string> &Get_path(void) const { return path; }
  bool operator<(const PathPattern &) const;
  bool operator<(std::string const &) const;
  friend bool operator<(std::string const &, PathPattern const &);
};

struct RouteRule {
  Http::Method method; // GET | POST | DELETE 등
  PathPattern path;    // "/old_stuff/*", "*.(jpg|jpeg|gif)" 등
  int status_code;     // 상태코드

  RuleOperator op;
  PathPattern redirectTarget;

  // 정적 파일 전용
  PathPattern root;                      // "/spool/www"
  std::string index;                     // "index2.html" (있으면)
  std::string authInfo;                  // "@auth_info"에서 추출
  int maxBodyKB;                        // "< 10MB" → 10 * 1024 * 1024
  std::map<int, std::string> errorPages; // 404 → "/404.html"
};

class ServerConfig {
private:
  Header header;
  int serverResponseTime;
  std::map<std::pair<Http::Method, PathPattern>, RouteRule> routes;
  std::string err_line;

  bool set_ServerConfig(FileDescriptor &fd);
  // header method
  bool is_header(const std::string &line);
  bool parse_header_line(FileDescriptor &fd, std::string line);
  bool is_header_key(std::string &key);
  bool parse_header_value(std::string value, const std::string key);
  // serverResponseTime method
  bool is_serverResponseTime(std::string &line);
  void parse_serverResponseTime(std::string line);
  // RouteRule method
  bool is_RouteRule(std::string line);
  bool parse_RouteRule(std::string line, FileDescriptor &fd);
  bool parse_Httpmethod(std::vector<std::string> data,
  std::vector<Http::Method> mets);
  bool parse_Rule(std::vector<Http::Method> met, std::string key,
    std::string line);
  RuleOperator parse_RuleOperator(std::string indicator);
      
public:
  ServerConfig(FileDescriptor &);
  ServerConfig() : header(), serverResponseTime(-1), routes(), err_line() {}
  const Header& Get_Header(void) const { return header; }
  int Get_ServerResponseTime(void) const { return(serverResponseTime); }
  const std::map<std::pair<Http::Method, PathPattern>, RouteRule>& Get_Routes (void) const { return routes; }
  const std::string& Geterr_line(void) const { return err_line; }
  // Result<ServerConfig> read_from_file(FileDescriptor &);
};

std::ostream& operator<<(std::ostream& os, const ServerConfig& data);
std::ostream& operator<<(std::ostream& os, const PathPattern& data);

#endif
