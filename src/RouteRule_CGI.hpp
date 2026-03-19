#ifndef ROUTERULE_CGI_HPP
#define ROUTERULE_CGI_HPP

#include "ParsingUtils.hpp"
#include "file_descriptor.h"
#include "http_1_1.h"
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @class RouteRule_CGI
 * @brief CGI 요청 처리를 위한 규칙과 실행 정보를 저장하는 클래스
 *
 * 요청 메서드, 요청 경로, 실행 파일 경로, 환경 변수, 실행 제한 시간과 같은
 * CGI 처리에 필요한 정보를 함께 관리한다.
 */
class RouteRule_CGI {
private:
  /**
   * @var met
   * @brief 이 규칙이 적용되는 HTTP 요청 메서드를 저장하는 멤버 변수
   */
  Http::Method met;
  /**
   * @var path
   * @brief 이 규칙이 적용되는 요청 경로를 저장하는 멤버 변수
   */
  std::string path;
  /**
   * @var executable
   * @brief 요청 처리에 사용할 CGI 실행 파일의 경로를 저장하는 멤버 변수
   */
  std::string executable;
  /**
   * @var env
   * @brief CGI 실행 시 사용할 환경 변수 정보를 저장하는 멤버 변수
   *
   * 환경 변수의 이름을 키로 하고, 해당 변수의 값을 값으로 저장한다.
   */
  std::map<std::string, std::string> env;
  /**
   * @var timeout
   * @brief CGI 실행의 제한 시간을 저장하는 멤버 변수
   */
  double timeout;
  /**
   * @var err
   * @brief CGI 규칙 파싱 또는 처리 중 발생한 오류 정보를 저장하는 멤버 변수
   */
  std::string err;

  std::string parse_cgi(FileDescriptor &fd, std::string line);
  bool is_timeout(const std::string &line);
  double parse_timeout(std::string &line);

public:
  RouteRule_CGI() : executable(""), env(), timeout(-1), err("No parse"){};
  RouteRule_CGI(FileDescriptor &fd, std::string);

  const std::string get_err() const { return err; }
  const std::string get_executable() const { return executable; }
  const std::map<std::string, std::string> get_env() const { return env; }
  double get_timeout() const { return timeout; }

  static std::string parse_config_uwsgi(FileDescriptor &fd,
                               std::map<std::string, std::string> &uwsgi);
  static bool is_config_cgi(std::string line);
  static bool is_cgi(const std::string &line);
  static bool is_executable_file(const std::string &path);
  static std::string parse_env(const std::string &,
                        std::map<std::string, std::string> &env);
  static std::string parse_executable(const std::string &line, std::string &executable,
                              std::map<std::string, std::string> &map);
};

std::ostream &operator<<(std::ostream &os, const RouteRule_CGI &data);

#endif