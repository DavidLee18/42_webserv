#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "ParsingUtils.hpp"
#include "http_1_1.h"

typedef std::map<std::string, std::map<std::string, std::string> > Header;
class Pathpattern;
enum RouteType { ROUTE_REDIRECT, ROUTE_STATIC, ROUTE_OTHER };


struct RouteRule {
  HttpMethod method; // GET | POST | DELETE 등
  std::vector<std::string> path; // "/old_stuff/*", "*.(jpg|jpeg|gif)" 등
  int status_code;  // 상태코드

  RuleOperator op;
  std::string redirectTarget;

  // 정적 파일 전용
  std::vector<std::string> root;                      // "/spool/www"
  std::string index;                     // "index2.html" (있으면)
  std::string authInfo;                  // "@auth_info"에서 추출
  long maxBodyMB;                     // "< 10MB" → 10 * 1024 * 1024
  std::map<int, std::string> errorPages; // 404 → "/404.html"
};

class Pathpattern
{
private:
  std::vector<std::string> path;

  bool match(std::string wildcard, std::string path)
  {
    std::vector<std::string> data = string_split(wildcard, "*");
    if (2 < data.size())
      return (false);
    for (size_t i = 0; i < data.size(); ++i)
    {
      size_t pos = std::string::npos;
      if (path.find(data[i]) == std::string::npos)
        return (false);
      else if (pos == std::string::npos + 1)
        return (false);
    }
    return (true);
  }
public:
  Pathpattern(std::vector<std::string> path) { this->path = path; };
  ~Pathpattern() {};

  bool operator==(std::string line)
  {
    std::vector<std::string> d_1 = this->path;
    std::vector<std::string> d_2 = string_split(line, "/");

    for (size_t i = 0; i < d_1.size(); ++i)
    {
      if (d_1[i] == d_2[i])
        continue;
      else if (d_1[i] == "*")
        continue;
      else if (d_1[i].find('*') != std::string::npos && match(d_1[i], d_2[i]))
        continue;
      else
        return (false);
    }
    return (true);
  }
  friend bool operator==(std::string line, Pathpattern &path) { return(path == line); }
};


class ServerConfig{
private:
  Header header;
  bool is_success;
  int serverResponseTime;
  std::map<std::pair<HttpMethod, Pathpattern>, RouteRule, std::less<>> routes;
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
  bool parse_RouteRule(std::string line, std::ifstream &file);
  bool parse_Httpmethod(std::vector<std::string> data, std::vector<HttpMethod> mets);
  bool parse_Rule(std::vector<HttpMethod> met, std::string key, std::string line);
  RuleOperator parse_RuleOperator(std::string indicator);

public:
  ServerConfig();
  ServerConfig(std::ifstream &file);
  ~ServerConfig();

  bool getis_succes(void);
};

#endif
