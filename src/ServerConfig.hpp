#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "ParsingUtils.hpp"
#include "http_1_1.h"

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

class Pathpattern {
private:
  std::vector<std::string> path;

  bool match(std::string wildcard, std::string path) const;

public:
  Pathpattern() : path() {}
  Pathpattern(std::vector<std::string> path) : path(path) {}

  bool operator==(const std::string &) const;
  friend bool operator==(const std::string &line, const Pathpattern &path) {
    return (path == line);
  }

  bool operator<(const Pathpattern &) const;
};

struct RouteRule {
  Http::Method method; // GET | POST | DELETE 등
  Pathpattern path;    // "/old_stuff/*", "*.(jpg|jpeg|gif)" 등
  int status_code;     // 상태코드

  RuleOperator op;
  std::string redirectTarget;

  // 정적 파일 전용
  Pathpattern root;                      // "/spool/www"
  std::string index;                     // "index2.html" (있으면)
  std::string authInfo;                  // "@auth_info"에서 추출
  long maxBodyMB;                        // "< 10MB" → 10 * 1024 * 1024
  std::map<int, std::string> errorPages; // 404 → "/404.html"
};

class ServerConfig {
private:
  Header header;
  int serverResponseTime;
  std::map<std::pair<Http::Method, Pathpattern>, RouteRule, std::less<> > routes;
  std::string err_line;

  ServerConfig() : header(), serverResponseTime(-1), routes(), err_line() {}
  // header method
  bool is_header(const std::string &line);
  bool parse_header_line(std::ifstream &file, std::string line);
  bool is_header_key(std::string &key);
  bool parse_header_value(std::string value, const std::string key);
  // serverResponseTime method
  bool is_serverResponseTime(std::string &line);
  void parse_serverResponseTime(std::string line);
  // RouteRule method
  bool is_RouteRule(std::string line);
  bool parse_RouteRule(std::string line, std::ifstream &file);
  bool parse_Httpmethod(std::vector<std::string> data,
                        std::vector<Http::Method> mets);
  bool parse_Rule(std::vector<Http::Method> met, std::string key,
                  std::string line);
  RuleOperator parse_RuleOperator(std::string indicator);

public:
  std::string Geterr_line(void);
};

#endif
