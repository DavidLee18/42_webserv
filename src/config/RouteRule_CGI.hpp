#ifndef ROUTERULE_CGI_HPP
#define ROUTERULE_CGI_HPP

#include "../file_descriptor.h"
#include "PathPattern.hpp"

/**
 * @class RouteRule_CGI
 * @brief CGI 요청 처리를 위한 규칙과 실행 정보를 저장하는 클래스
 *
 * - 요청 메서드, 요청 경로, 실행 파일 경로, 환경 변수,
 * 실행 제한 시간과 같은 CGI 처리에 필요한 정보를 함께 관리한다.
 */
class RouteRule_CGI {
  /**
   * @var met
   * @brief 이 규칙이 적용되는 HTTP 요청 메서드를 저장하는 멤버 변수
   */
  Request::Method met;
  /**
   * @var path
   * @brief 이 규칙이 적용되는 요청 경로를 저장하는 멤버 변수
   */
  PathPattern path;
  /**
   * @var executable
   * @brief 요청 처리에 사용할 CGI 실행 파일의 경로를 저장하는 멤버 변수
   */
  std::string executable;
  /**
   * @var env
   * @brief CGI 실행 시 사용할 환경 변수 정보를 저장하는 멤버 변수
   *
   * - 환경 변수의 이름을 키로 하고, 해당 변수의 값을 값으로 저장한다.
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
   *
   * - 정상적으로 파싱이 성공했을 시 빈 문자열을 가지고 있다.
   */
  std::string err;

  /**
   * @brief 문자열이 timeout 문법과 값 범위에 맞는지 확인하는 함수
   * @param line 검사할 문자열
   * @return timeout 문법과 값 범위에 맞으면 true, 그렇지 않으면 false
   *
   * - 문법은 "...<숫자 문자열>" 형식이다.
   *
   * - 숫자 문자열의 값은 0.05보다 크고 15.0 이하여야 한다.
   */
  static bool is_valid_timeout(const std::string &line);
  /**
   * @brief 검증된 timeout 문자열을 double 값으로 변환하는 함수
   * @param line 변환할 문자열
   * @return 0.05보다 크고 15.0 이하인 timeout 값
   *
   * - 입력 문자열은 is_valid_timeout(const std::string &line) 함수로
   * 유효성이 확인된 상태여야 한다.
   */
  static double parse_timeout_value(std::string &line);
  /**
   * @brief CGI 설정 블록을 파싱하여 실행 파일, 환경 변수, timeout 정보를
   * 저장하는 함수
   * @param fd 설정 파일을 읽기 위한 FileDescriptor
   * @param line CGI 설정 블록의 첫 줄
   * @return 성공하면 빈 문자열, 실패하면 오류 메시지
   *
   * - 첫 줄에서는 실행 파일 경로와 선택적인 환경 변수 정보를 파싱한다.
   *
   * - 이후 들여쓰기 2단계의 하위 줄에서 timeout 또는 추가 환경 변수 정보를
   * 읽는다.
   */
  std::string parse_cgi_block(const FileDescriptor &fd, const std::string& line);

public:
  RouteRule_CGI() : met(Request::ERROR), timeout(-1), err("No parse") {}
  /**
   * @brief 검증된 CGI 설정 한 줄을 바탕으로 RouteRule_CGI 객체를 생성하는
   * 생성자
   * @param fd 설정 파일을 읽기 위한 FileDescriptor
   * @param line CGI 규칙의 첫 줄
   *
   * - 요청 메서드와 경로를 설정한 뒤, 하위 CGI 블록을 파싱하여
   * 실행 파일, 환경 변수, timeout 정보를 초기화한다.
   */
  RouteRule_CGI(const FileDescriptor &fd, const std::string &line);

  PathPattern get_path() const { return path; }
  Request::Method get_method () const { return met; }
  std::string get_err() const { return err; }
  std::string get_executable() const { return executable; }
  std::map<std::string, std::string> get_env() const { return env; }
  double get_timeout() const { return timeout; }

  /**
   * @brief 문자열이 환경 변수 이름 문법에 맞는지 검사하는 함수
   * @param key 검사할 문자열
   * @return 환경 변수 이름으로 사용할 수 있으면 true, 그렇지 않으면 false
   *
   * - 문자열은 비어 있을 수 없으며,
   * 첫 번째 문자는 대문자 또는 '_'이어야 한다.
   *
   * - 나머지 문자는 대문자, 숫자, '_'만 허용한다.
   */
  static bool is_valid_env_key(const std::string &key);
  /**
   * @brief 문자열 벡터가 유효한 uwsgi 설정 값인지 검사하는 함수
   * @param data 검사할 문자열 벡터
   * @return 유효한 uwsgi 설정 값이면 true, 그렇지 않으면 false
   *
   * - 입력 벡터의 크기는 2여야 한다.
   *
   * - 첫 번째 원소는 실행 가능한 파일 경로여야 하고,
   * 두 번째 원소는 포트 번호를 나타내는 숫자 문자열이어야 한다.
   */
  static bool is_valid_uwsgi_config(const std::vector<std::string> &data);
  /**
   * @brief CGI 설정 한 줄의 기본 형식을 검사하는 함수
   * @param line 검사할 문자열
   * @return 기본 형식이 유효하면 true, 그렇지 않으면 false
   *
   * - 입력 문자열은 공백 기준으로 세 개의 항목으로 나뉘어야 한다.
   *
   * - 첫 번째 항목은 HTTP 메서드, 두 번째 항목은 공백이 없는 URL이어야 한다.
   */
  static bool is_valid_cgi_config(const std::string &line);
  /**
   * @brief 문자열이 유효한 CGI 설정 형식인지 검사하는 함수
   * @param line 검사할 문자열
   * @return 유효한 CGI 설정 형식이면 true, 그렇지 않으면 false
   *
   * - 입력 문자열은 '$'로 시작해야 하며 공백을 포함할 수 없다.
   *
   * - "$<숫자 문자열>" 또는 "$<확장자가 .cgi인 실행 파일>" 뒤에
   * 선택적으로 "(키=값)" 형식의 문자열이 올 수 있다.
   */
  static bool matches_cgi_syntax(const std::string &line);
  /**
   * @brief 주어진 경로가 실행 가능한 파일인지 검사하는 함수
   * @param path 검사할 실행 파일 경로
   * @return 실행 가능하면 true, 그렇지 않으면 false
   *
   * - 파일이 존재해야 하며, 일반 파일이어야 하고, 실행 권한이 있어야 한다.
   */
  static bool is_executable_file(const std::string &path);
  /**
   * @brief 환경 변수 한 줄을 파싱하여 env 맵에 추가하는 함수
   * @param line 파싱할 문자열
   * @param env 파싱 결과를 저장할 환경 변수 맵
   * @return 파싱에 성공하면 빈 문자열, 실패하면 오류 메시지
   *
   * - 입력 문자열은 "key=value" 형식이어야 하며,
   * key는 유효한 환경 변수 이름이어야 하고 중복될 수 없다.
   */
  static std::string parse_env_entry(const std::string &line,
                                     std::map<std::string, std::string> &env);
  /**
   * @brief CGI 실행 문자열에서 실행 파일 경로와 선택적인 환경 변수 정보를
   * 추출하는 함수
   * @param line 파싱할 문자열
   * @param executable 파싱한 실행 파일 경로를 저장할 변수
   * @param env 파싱한 환경 변수 정보를 저장할 변수
   * @return 성공하면 빈 문자열, 실패하면 오류 메시지
   *
   * - 입력 문자열은 is_valid_cgi_config(const std::string &line) 또는
   * matches_cgi_syntax(const std::string &line) 함수로 유효성이 확인된 상태여야
   * 한다.
   */
  static std::string parse_executable(const std::string &line,
                                      std::string &executable,
                                      std::map<std::string, std::string> &env);
  /**
   * @brief uwsgi 설정 블록을 파싱하여 포트 번호를 키로, 실행 파일 경로를 값으로
   * 저장하는 함수
   * @param fd 설정 파일을 읽기 위한 FileDescriptor
   * @param uwsgi 파싱한 uwsgi 설정 정보를 저장할 변수
   * @return 성공하면 빈 문자열, 실패하면 오류 메시지
   *
   * - 빈 줄 또는 파일 끝을 만나면 파싱을 종료한다.
   *
   * - 각 항목은 유효한 uwsgi 설정 형식을 따라야 하며, 실행 파일은 .py 확장자를
   * 가져야 한다.
   */
  static std::string
  parse_uwsgi_block(const FileDescriptor &fd,
                    std::map<std::string, std::string> &uwsgi);
};

std::ostream &operator<<(std::ostream &os, const RouteRule_CGI &data);

#endif
