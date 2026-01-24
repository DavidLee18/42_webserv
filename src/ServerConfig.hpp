#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "ParsingUtils.hpp"
#include "http_1_1.h"

typedef std::map<std::string, std::map<std::string, std::string> > Header;

enum RouteType { ROUTE_REDIRECT, ROUTE_STATIC, ROUTE_OTHER };

struct RouteRule {
  RouteType type;

  std::vector<Http::Method> methods; // GET | POST | DELETE 등
  std::string urlPattern;          // "/old_stuff/*", "*.(jpg|jpeg|gif)" 등

  // 리다이렉트 전용
  int redirectStatus;         // 301
  std::string redirectTarget; // "/new_stuff/*", "/*/mp3/*.mp3"

  // 정적 파일 전용
  std::string root;                      // "/spool/www"
  std::string index;                     // "index2.html" (있으면)
  std::string authInfo;                  // "@auth_info"에서 추출
  long maxBodyBytes;                     // "< 10MB" → 10 * 1024 * 1024
  std::map<int, std::string> errorPages; // 404 → "/404.html"
};

class ServerConfig {
private:
  Header header;
  bool is_success;
  int serverResponseTime;
  std::vector<RouteRule> routes;
  std::vector<std::string> err_line;

  bool set_ServerConfig(std::ifstream &file);
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

public:
  ServerConfig();
  ServerConfig(std::ifstream &file);
  ~ServerConfig();

  bool getis_succes(void);
};

#endif
