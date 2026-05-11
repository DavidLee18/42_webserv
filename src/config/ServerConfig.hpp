#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "RouteRule_CGI.hpp"

/**
 * @typedef Server_CGI
 * @brief server 단위 CGI 항목과 그에 대응하는 메타변수 정보를 저장하는 중첩 map
 * 타입
 *
 * - 바깥 map은 CGI 항목을 구분하는 키를 사용한다.
 *
 * - 내부 map은 메타변수 이름을 키로, 그 값을 값으로 저장한다.
 */
typedef std::map<std::string, std::map<std::string, std::string> > CGI;

/**
 * @enum RuleOperator
 * @brief rewrite 규칙에서 사용되는 연산자 종류를 정의한 열거형
 *
 * - 리다이렉트 상태 코드와 경로 변환 규칙을 구분하기 위해 사용한다.
 */
enum RuleOperator {
  /**
   * @brief 300 Multiple Choices 상태 코드를 나타낸다.
   */
  MULTIPLE_CHOICES,
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
  SEE_OTHER,
  /**
   * @brief 304 Not Modified 상태 코드를 나타낸다.
   */
  NOT_MODIFIED,
  /**
   * @brief 307 Temporary Redirect 리다이렉트를 나타낸다.
   */
  TEMPORARY_REDIRECT,
  /**
   * @brief 308 Permanent Redirect 리다이렉트를 나타낸다.
   */
  PERMANENT_REDIRECT,
  /**
   * @brief 디렉토리의 autoindex 동작을 나타낸다.
   */
  AUTOINDEX,
  /**
   * @brief 특정 경로로 업로드하는 규칙을 나타낸다.
   */
  UPLOAD_TO,
  /**
   * @brief 특정 경로로부터 파일을 제공하는 규칙을 나타낸다.
   */
  SERVE_FROM,
  /**
   * @brief 정의되지 않은 연산자 상태를 나타낸다.
   */
  UNDEFINED,
};

/**
 * @struct RouteRule
 * @brief 설정 파일에 정의된 경로 처리 규칙과 하위 설정 정보를 저장하는 구조체
 *
 * - 상위 규칙에는 요청 메서드, 경로 패턴, 처리 연산자 및 대상 경로가 포함된다.
 *
 * - 하위 설정에는 index, auth, body size, error page 등의 추가 정보가 포함될 수
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
   * @var upload_dir
   * @brief 클라이언트 파일 업로드를 허용할 디렉토리 경로를 저장하는 멤버 변수
   *
   * - 빈 문자열이면 해당 규칙에서는 파일 업로드가 비활성화된다.
   */
  std::string upload_dir;
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
   * - 상태 코드를 키로 하고, 해당 상태 코드에 대응하는 오류 페이지 경로를
   * 값으로 저장한다.
   */
  std::map<int, std::string> error_pages;
};

/**
 * @class ServerConfig
 * @brief 설정 파일에서 파싱한 서버 설정 정보를 저장하는 클래스
 *
 * - 서버 공통 설정과 라우팅 규칙, CGI 설정, 파싱 상태 정보를 포함된다.
 *
 * - 하나의 서버 설정 단위를 표현한다.
 */
class ServerConfig {
  /**
   * @var header
   * @brief server 블록의 header 정보를 저장하는 멤버 변수
   */
  std::map<std::string, std::string> header;
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
   * @var err_line
   * @brief 파싱 중 오류가 발생한 설정 파일의 줄 정보를 저장하는 멤버 변수
   */
  std::string err_meg;
  /**
   * @var end_flag
   * @brief server 블록 종료 판단을 위한 상태값을 저장하는 멤버 변수
   */
  int end_flag;
  std::size_t count_line;

  /**
   * @brief server 블록의 최상위 설정 항목들을 파싱하는 함수
   * @param fd 설정 파일을 읽기 위한 FileDescriptor
   * @return 파싱에 성공하면 true, 실패하면 false
   *
   * - server 블록 내부의 header, CGI 설정, 응답 시간, RouteRule, RouteRule_CGI
   * 항목을 순차적으로 읽어 각 멤버 변수에 저장한다.
   *
   * - 파싱 중 오류가 발생하면 err_line에 오류 메시지를 저장한다.
   */
  bool parse_server_block(const FileDescriptor &fd);
  /**
   * @brief 문자열이 "[] +<=" 형식의 header 설정 시작 줄인지 검사하는 함수
   * @param line 검사할 문자열
   * @return header 설정 시작 줄이면 true, 그렇지 않으면 false
   */
  static bool is_header_block(const std::string &line);
  /**
   * @brief header 항목을 파싱하여 key와 value를 header 맵에 저장하는 함수
   * @param fd 설정 파일을 읽기 위한 FileDescriptor
   * @param line 파싱할 header 항목 문자열
   * @return 파싱에 성공하면 true, 실패하면 false
   *
   * - line은 is_header_block(const std::string &line) 함수로
   * 사전에 검증된 문자열이어야 한다.
   *
   * - 값이 ';'로 끝나면 다음 들여쓰기 2단계 줄들을 이어 읽어
   * 하나의 값으로 처리한다.
   *
   * - 파싱 중 오류가 발생하면 err_line에 오류 메시지를 저장한다.
   */
  bool parse_header_entry(const FileDescriptor &fd, const std::string &line);
  /**
   * @brief 문자열이 server response time 문법과 범위 조건에 맞는지 검사하는
   * 함수
   * @param line 검사할 문자열
   * @return 문법과 범위 조건에 맞으면 true, 그렇지 않으면 false
   *
   * - 입력 문자열은 "...<숫자 문자열>" 형식을 따라야 한다.
   *
   * - 숫자 값은 1 이상 900 이하여야 한다.
   */
  static bool is_valid_server_response_time(const std::string &line);
  /**
   * @brief 검증된 server response time 문자열에서 숫자 값을 추출하여
   * server_response_time에 저장하는 함수
   * @param line 파싱할 문자열
   *
   * - 입력 문자열은 is_valid_server_response_time(const std::string &line)
   * 함수로 사전에 검증된 문자열이어야 한다.
   */
  void parse_server_response_time(std::string line);
  /**
   * @brief 문자열이 "*.(a|b|...)" 형식의 경로 패턴 요소인지 검사하는 함수
   * @param line 검사할 문자열
   * @return 유효한 패턴 요소이면 true, 그렇지 않으면 false
   *
   * - 괄호 안에는 '|'로 구분된 두 개 이상의 확장자 후보가 있어야 한다.
   *
   * - 각 후보는 영숫자로만 구성되어야 한다.
   */
  static bool is_path_pattern_segment(const std::string &line);
  /**
   * @brief "*.(a|b|...)" 형식의 패턴 요소에서 괄호 안 후보 목록을 추출하는 함수
   * @param line 추출할 패턴 문자열
   * @return '|'를 기준으로 분리된 후보 문자열 목록
   */
  static std::vector<std::string>
  get_pattern_candidates(const std::string &line);
  /**
   * @brief 기존 경로 조합의 특정 위치에 패턴 후보들을 적용하여 모든 조합을
   * 생성하는 함수
   * @param paths 기존 경로 조합 목록
   * @param pattern 적용할 후보 문자열 목록
   * @param index 치환할 경로 요소의 위치
   * @return 패턴이 적용된 새로운 경로 조합 목록
   *
   * - 원래 경로 요소의 prefix와 suffix는 유지하고,
   * 가운데 패턴 후보 부분만 교체하여 새 경로들을 생성한다.
   */
  static std::vector<PathPattern>
  expand_paths_with_pattern(const std::vector<PathPattern> &paths,
                            const std::vector<std::string> &pattern,
                            std::size_t index);
  /**
   * @brief 경로 패턴 문자열을 분해하고 패턴 요소를 확장하여 경로 조합 목록으로
   * 반환하는 함수
   * @param line 확장할 경로 패턴 문자열
   * @return 확장된 경로 조합 목록
   *
   * - 경로는 '/'를 기준으로 분리된다.
   *
   * - 패턴 요소가 포함된 경우 가능한 모든 조합으로 확장된다.
   */
  static std::vector<PathPattern> expand_path_pattern(const std::string &line);
  /**
   * @brief URL 패턴 문자열에서 경로 요소별 '*' 사용 규칙을 검사하는 함수
   * @param url 검사할 URL 패턴 문자열
   * @return 모든 경로 요소가 규칙을 만족하면 true, 그렇지 않으면 false
   *
   * - '*' 문자는 같은 경로 요소 안에서 두 번 이상 사용할 수 없다.
   *
   * - '/'를 만나면 다음 경로 요소에 대한 검사를 새로 시작한다.
   */
  static bool has_valid_wildcard_usage(const std::string &url);
  /**
   * @brief 최대 요청 바디 크기 문자열을 KB 단위 정수 값으로 변환하는 함수
   * @param line 파싱할 문자열
   * @return 변환에 성공하면 KB 단위 크기, 실패하면 -1
   *
   * - 단위가 없거나 KB, KiB이면 그대로 사용한다.
   *
   * - MB와 MiB는 각각 1000배, 1024배로 변환한다.
   */
  std::string parse_max_body_size(std::string line, int &maxbody);
  /**
   * @brief 문자열이 RouteRule 시작 줄 형식에 맞는지 검사하는 함수
   * @param line 검사할 문자열
   * @return RouteRule 시작 줄 형식이면 true, 그렇지 않으면 false
   *
   * - 문자열은 "<Method> <URL> <Operator> <URL>" 형식이어야 한다.
   *
   * - Method에는 GET, POST, DELETE를 '|'로 구분하여 하나 이상 지정할 수 있다.
   */
  static bool matches_route_rule_syntax(const std::string &line);
  /**
   * @brief path 패턴과 root 패턴의 와일드카드 위치가 호환되는지 검사하는 함수
   * @param path 검사할 path 패턴
   * @param root 검사할 root 패턴
   * @return 두 패턴의 와일드카드 구성이 호환되면 true, 그렇지 않으면 false
   *
   * - path에 포함된 와일드카드가 root에서도 대응되는 위치를 가져야 한다.
   */
  static bool has_compatible_wildcards(const PathPattern &path,
                                       const PathPattern &root);
  /**
   * @brief RouteRule 블록을 파싱하여 규칙 정보를 저장하는 함수
   * @param method_line RouteRule 블록의 시작 줄
   * @param fd 설정 파일을 읽기 위한 FileDescriptor
   * @return 파싱에 성공하면 true, 실패하면 false
   *
   * - 시작 줄에서 HTTP 메서드와 경로 정보를 추출한 뒤,
   * 들여쓰기 2단계의 하위 줄들을 읽어 각 규칙 항목을 파싱한다.
   *
   * - 파싱 중 오류가 발생하면 err_line에 오류 메시지를 저장한다.
   */
  bool parse_route_rule_block(const std::string &method_line,
                              const FileDescriptor &fd);
  /**
   * @brief RouteRule 시작 줄 정보를 바탕으로 route 규칙들을 생성하는 함수
   * @param data "<Method> <URL> <Operator> <URL>" 형식의 RouteRule 시작 줄을
   * 공백 기준으로 분리한 문자열 목록
   * @param mets 적용할 HTTP 메서드 목록
   * @return 생성에 성공하면 true, 실패하면 false
   *
   * - URL 패턴을 확장하여 각 메서드와 경로 조합에 대한 RouteRule을 생성한다.
   *
   * - 연산자 종류에 따라 root 또는 redirect_target을 설정한다.
   */
  bool create_route_rules(const std::vector<std::string> &data,
                          const std::vector<Request::Method> &mets);
  /**
   * @brief RouteRule 하위 설정 항목을 파싱하여 해당 메서드와 경로의 routes에
   * 적용하는 함수
   * @param mets 규칙이 적용될 HTTP 메서드 목록
   * @param key_data 규칙이 적용될 경로 패턴 문자열
   * @param line 파싱할 하위 규칙 문자열
   * @return 파싱에 성공하면 true, 실패하면 false
   *
   * - 동일한 메서드와 경로를 가진 RouteRule이 이미 존재하면 해당 객체를
   * 갱신한다.
   *
   * - 존재하지 않으면 새 RouteRule을 생성한 뒤 규칙을 적용한다.
   */
  bool apply_route_rule_entry(const std::vector<Request::Method> &mets,
                              const std::string &key_data,
                              const std::string &line);
  /**
   * @brief 연산자 문자열을 RuleOperator 열거형 값으로 변환하는 함수
   * @param indicator 변환할 연산자 문자열
   * @return 변환된 RuleOperator 값, 유효하지 않으면 UNDEFINED
   */
  static RuleOperator parse_rule_operator(const std::string &indicator);

public:
  explicit ServerConfig(FileDescriptor &);
  ServerConfig()
      : header(), server_response_time(-1), routes(), err_meg(), end_flag(0) {}
  /**
   * @brief Request method와 path에 일치하는 route를 찾는다.
   * @param method 요청 HTTP 메서드
   * @param path 요청 경로
   * @return 일치하는 RouteRule의 포인터, 없으면 NULL
   *
   * - 등록된 route 목록을 순회하면서, 전달된 HTTP method와 path에
   * 모두 일치하는 첫 번째 route를 반환한다.
   */
  RouteRule const *find_route(Request::Method method,
                              const std::string &path) const;
  RouteRule_CGI const *find_route_cgi(Request::Method method,
                                      const std::string &path) const;
  /**
   * @brief method와 path에 해당하는 rewrite 결과 경로를 반환한다.
   * @param method 요청 HTTP 메서드
   * @param path 요청 경로
   * @return 재작성된 대상 경로, 일치하는 route가 없으면 빈 문자열
   *
   * - 먼저 method와 path에 일치하는 route를 찾고, 해당 route의 패턴 정보로
   * 요청 경로를 대상 경로로 재작성한다.
   */
  std::string get_rewritten_path(Request::Method method,
                                 const std::string &path) const;

  const std::map<std::string, std::string> &get_header() const {
    return header;
  }
  const std::string &get_err_meg(void) const { return err_meg; }
  const std::vector<RouteRule_CGI> get_route_rule_cgi() const { return R_CGI; }
  const std::vector<RouteRule> &get_routes(void) const { return routes; }
  std::size_t get_count_line(void) const { return count_line; }
  int get_server_response_time(void) const { return server_response_time; }
  static std::string
  apply_default_err_page_entry(const std::string &line,
                               std::map<int, std::string> &err_map);
};

std::ostream &operator<<(std::ostream &os, const ServerConfig &data);
std::ostream &operator<<(std::ostream &os, const PathPattern &data);

#endif
