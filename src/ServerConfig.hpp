#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "ParsingUtils.hpp"
#include "RouteRule_CGI.hpp"
#include "file_descriptor.h"
#include "server/Client.hpp"
#include <iosfwd>
#include <unistd.h>

typedef std::map<std::string, std::map<std::string, std::string> > Header;
/**
 * @typedef Server_CGI
 * @brief CGI 관련 메타변수 정보를 저장하기 위한 중첩 map 타입
 *
 * 바깥 map은 실행 파일의 경로 또는 포트 번호를 키로 사용하고,
 * 내부 map은 해당 항목에 대한 메타변수의 이름을 키로,
 * 메타변수의 값을 저장한다.
 */
typedef std::map<std::string, std::map<std::string, std::string> > Server_CGI;

/**
 * @enum RuleOperator
 * @brief rewrite 규칙에서 사용되는 연산자 종류를 정의한 열거형
 *
 * 리다이렉트 상태 코드와 경로 변환 규칙을 구분하기 위해 사용한다.
 */
enum RuleOperator {
  /**
   * @brief 300 Multiple Choices 상태 코드를 나타낸다.
   */
  MULTIPLECHOICES,
  /**
   * @brief 301 Moved Permanently 리다이렉트를 나타낸다.
   */
  REDIRECT,
  /**
   * @brief 302 Found 리다이렉트를 나타낸다.
   */
  FOUND,
  /**
   * @brief 303 See Other 리다이렉트를 나타낸다.
   */
  SEEOTHER,
  /**
   * @brief 304 Not Modified 상태 코드를 나타낸다.
   */
  NOTMODIFIED,
  /**
   * @brief 307 Temporary Redirect 리다이렉트를 나타낸다.
   */
  TEMPORARYREDIRECT,
  /**
   * @brief 308 Permanent Redirect 리다이렉트를 나타낸다.
   */
  PERMANENTREDIRECT,
  /**
   * @brief 디렉토리의 autoindex 동작을 나타낸다.
   */
  AUTOINDEX,
  /**
   * @brief 현재 경로를 기준으로 연결하는 규칙을 나타낸다.
   */
  POINT,
  /**
   * @brief 특정 경로로부터 파일을 제공하는 규칙을 나타낸다.
   */
  SERVEFROM,
  /**
   * @brief 정의되지 않은 연산자 상태를 나타낸다.
   */
  UNDEFINED,
};

/**
 * @class PathPattern
 * @brief 설정 파일의 경로 패턴을 분리하여 저장하고 요청 URL과의 매칭에 사용하는
 * 클래스
 *
 * 경로를 '/' 단위로 분리하여 각 요소를 비교할 수 있도록 관리한다.
 * 이때 '*' 문자는 와일드카드로 해석하며, 해당 위치의 임의의 경로 문자열과
 * 일치하는 것으로 처리한다.
 *
 * 예를 들어,
 * 설정 파일에 /download/{wildcard}/mp3/{wildcard} 와 같은 패턴이 정의되어 있을
 * 때, 요청 URL이 /download/asd/mp3/asd 이면 동일한 경로 패턴으로 판단할 수
 * 있다.
 */
class PathPattern {
private:
  /**
   * @var path
   * @brief 설정 파일의 경로 패턴을 분리하여 저장하는 멤버 변수
   */
  std::vector<std::string> path;
  static bool segmentMatches(const std::string &pattern,
                             const std::string &segment);

public:
  PathPattern() : path() {}
  PathPattern(const std::string &pathStr)
      : path(utils::string_split(pathStr, "/")) {}
  PathPattern(std::vector<std::string> path) : path(path) {}

  void add_path(std::string data) {
    path.push_back(data);
    return;
  }
  bool is_wildcard() const { return (path.size() == 1 && path[0] == "*"); }
  bool matches(const PathPattern &other) const;
  bool matches(const std::string &pathStr) const;
  const std::vector<std::string> &get_path(void) const { return path; }
  std::string to_string() const;
};

/**
 * @struct RoteRule
 * @brief  설정 파일에 정의된 경로 처리 규칙과 하위 설정 정보를 저장하는 구조체
 *
 * 상위 규칙에는 요청 메서드, 경로 패턴, 처리 연산자 및 대상 경로가 포함되며,
 * 하위 설정에는 index, auth, body size, error page 등의 추가 정보가 포함될 수
 * 있다.
 */
struct RouteRule {
  /**
   * @var method
   * @brief 이 규칙이 적용되는 HTTP 요청 메서드를 저장하는 멤버 변수
   */
  Request::Method method;
  /**
   * @var path
   * @brief 이 규칙이 적용되는 요청 경로 패턴을 저장하는 멤버 변수
   */
  PathPattern path;
  /**
   * @var op
   * @brief 요청 경로에 대해 수행할 처리 연산자를 저장하는 멤버 변수
   */
  RuleOperator op;
  /**
   * @var redirect_target
   * @brief 연산자 적용 시 대상이 되는 경로 패턴을 저장하는 멤버 변수
   */
  PathPattern redirect_target;
  /**
   * @var root
   * @brief 요청 경로를 실제 파일 시스템 경로로 매핑할 때 사용하는 기준 경로를
   * 저장하는 멤버 변수
   */
  PathPattern root;
  /**
   * @var index
   * @brief 디렉토리 요청 시 기본으로 제공할 인덱스 파일 이름을 저장하는 멤버
   * 변수
   */
  std::string index;
  /**
   * @var auth_info
   * @brief 해당 규칙에 적용되는 인증 정보를 저장하는 멤버 변수
   */
  std::string auth_info;
  /**
   * @var max_body_KB
   * @brief 해당 규칙에서 허용하는 최대 요청 바디 크기를 KB 단위로 저장하는 멤버
   * 변수
   */
  int max_body_KB;
  /**
   * @var error_pages
   * @brief HTTP 상태 코드별 오류 페이지 경로를 저장하는 멤버 변수
   *
   * 상태 코드를 키로 하고, 해당 상태 코드에 대응하는 오류 페이지 경로를 값으로
   * 저장한다.
   */
  std::map<int, std::string> error_pages;
};

/**
 * @class ServerConfig
 * @brief 설정 파일에서 파싱한 서버 설정 정보를 저장하는 클래스
 *
 * 서버 공통 설정과 라우팅 규칙, CGI 설정, 파싱 상태 정보를 포함하며,
 * 하나의 서버 설정 단위를 표현한다.
 */
class ServerConfig {
private:
  /**
   * @var header
   * @brief 서버 설정의 공통 헤더 정보를 저장하는 멤버 변수
   */
  Header header;
  /**
   * @var server_response_time
   * @brief 서버의 응답 시간을 저장하는 멤버 변수
   */
  int server_response_time;
  /**
   * @var routes
   * @brief 일반 요청에 대한 경로 처리 규칙들을 저장하는 멤버 변수
   */
  std::vector<RouteRule> routes;
  /**
   * @var R_CGI
   * @brief CGI 요청에 대한 경로 처리 규칙들을 저장하는 멤버 변수
   */
  std::vector<RouteRule_CGI> R_CGI;
  /**
   * @var S_CGI
   * @brief 서버 단위의 CGI 관련 설정 정보를 저장하는 멤버 변수
   */
  Server_CGI S_CGI;
  /**
   * @var err_line
   * @brief 파싱 중 오류가 발생한 설정 파일의 줄 정보를 저장하는 멤버 변수
   */
  std::string err_line;
  /**
   * @var end_flag
   * @brief 설정 파일 파싱 종료 여부를 나타내는 멤버 변수
   */
  int end_flag;

  bool set_ServerConfig(FileDescriptor &fd);
  // header method
  bool is_header(const std::string &line);
  bool parse_header_line(FileDescriptor &fd, std::string line);
  bool is_header_key(std::string &key);
  bool parse_header_value(std::string value, const std::string key);
  // server_response_time method
  bool is_server_response_time(std::string &line);
  void parse_server_response_time(std::string line);
  // RouteRule method
  bool is_RouteRule(std::string line);
  bool is_matching(PathPattern path, PathPattern root);
  bool parse_RouteRule(std::string line, FileDescriptor &fd);
  bool parse_Httpmethod(std::vector<std::string> data,
                        std::vector<Request::Method> mets);
  bool parse_rule(std::vector<Request::Method> met, std::string key,
                  std::string line);
  RuleOperator parse_RuleOperator(std::string indicator);
  std::string rewrite_to(std::string from, PathPattern path,
                         PathPattern to) const;

public:
  ServerConfig(FileDescriptor &);
  ServerConfig()
      : header(), server_response_time(-1), routes(), err_line(), end_flag(0) {}
  const Header &get_header(void) const { return header; }
  int get_server_response_time(void) const { return (server_response_time); }
  const std::vector<RouteRule> &get_routes(void) const { return routes; }
  RouteRule const *find_route(Request::Method method,
                              const std::string &path) const;
  std::string get_to(Request::Method method, const std::string &path) const;
  const std::string &geterr_line(void) const { return err_line; }
  std::vector<RouteRule_CGI> get_route_rule_cgi() const { return R_CGI; }
  Server_CGI get_serve_cgi() const { return S_CGI; }
  // Result<ServerConfig> read_from_file(FileDescriptor &);
};

std::ostream &operator<<(std::ostream &os, const ServerConfig &data);
std::ostream &operator<<(std::ostream &os, const PathPattern &data);

#endif
