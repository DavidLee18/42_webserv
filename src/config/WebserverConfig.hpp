#ifndef WEBSERVERCONFIG_HPP
#define WEBSERVERCONFIG_HPP

#include "ServerConfig.hpp"



/**
 * @class WebserverConfig
 * @brief 웹서버 설정 파일을 파싱하고 그 결과를 멤버 변수에 저장하는 클래스
 *
 * - 설정 파일에서 Server, Type, Uwsgi 등의 항목을 읽어들인 뒤
 * 각 설정값을 내부 멤버 변수에 저장하고, 이후 웹서버가 해당
 * 설정 정보를 사용할 수 있도록 제공한다.
 *
 * - 이 클래스는 일반적인 방식으로 직접 인스턴스화할 수 없으며,
 * 정적 함수 parse(FileDescriptor &file)가 반환하는
 * Result<WebserverConfig> 를 통해 객체를 획득할 수 있다.
 * 
 * - 획득한 객체는 복사 또는 대입하여 사용할 수 있다.
 */
class WebserverConfig {
private:
  /**
   * @var err_meg
   * @brief 파싱 중 발생한 오류 메시지를 저장하는 멤버 변수
   *
   * - 정상적으로 파싱이 성공했을 시 빈 문자열을 가지고 있다.
   */
  std::string err_meg;
  /**
   * @var default_mime
   * @brief 설정 파일의 기본 MIME type 값을 저장하는 멤버 변수
   */
  std::string default_mime;
  /**
   * @var uwsgi
   * @brief 설정 파일의 uwsgi 항목 정보를 저장하는 멤버 변수
   *
   * - 포트 번호를 키로 하고, 실행 파일의 경로를 값으로 저장한다.
   */
  std::map<std::string, std::string> uwsgi;
  /**
   * @var type_map
   * @brief 파일 확장자와 MIME type의 매핑 정보를 저장하는 멤버 변수
   *
   * - 파일 확장자를 키로 하고, 해당 확장자에 대응하는 MIME type을 값으로
   * 저장한다.
   * 
   * - 매핑되지 않은 확장자에 대해서는 멤버 변수 default_mime에 저장된
   * 기본 MIME type을 사용한다.
   */
  std::map<std::string, std::string> type_map;
  /**
   * @var serverconfig_map
   * @brief  설정 파일에서 파싱한 server 설정 정보를 저장하는 멤버 변수
   *
   * - 포트 번호를 키로 하고,
   * 해당 포트에 대응하는 server 설정 정보를 값으로 저장한다.
   */
  std::map<unsigned int, ServerConfig> serverconfig_map;
  /**
   * @brief 서버의 기본 에러 응답 설정
   *
   * - 정적 에러 페이지 매핑과 CGI 기반 에러 처리 정보를 포함하는
   * 기본 에러 페이지 설정 객체이다.
   */
  std::map<int, std::string> default_err_page;

  /**
   * @brief 설정 파일의 최상위 항목들을 파싱하는 함수
   * @param file 파싱할 설정 파일
   * @return 파싱에 성공하면 true, 실패하면 false
   *
   * - types, server, uwsgi 항목을 순차적으로 읽어 각 멤버 변수에 저장한다.
   * 
   * - 유효하지 않은 줄이나 파싱 오류가 발생하면 err_meg에 오류 메시지를 저장한다.
   */
  bool file_parsing(FileDescriptor &file);
  /**
   * @brief types 블록을 파싱하여 확장자별 MIME type과 기본 MIME type을 저장하는
   * 함수
   * @param file 파싱할 설정 파일
   * @return 파싱에 성공하면 true, 실패하면 false
   *
   * - '_' 키는 기본 MIME type으로 처리된다.
   * 
   * - 그 외의 키는 type_map에 확장자별 MIME type으로 저장된다.
   */
  bool parse_types_block(FileDescriptor &file);
  /**
   * @brief "key1|key2->value" 형식의 type 매핑 문자열을 파싱하는 함수
   * @param line 파싱할 문자열
   * @param keys_out 파싱한 키 목록을 저장할 변수
   * @param value_out 파싱한 MIME type 값을 저장할 변수
   * @return 파싱에 성공하면 true, 실패하면 false
   *
   * - 입력 문자열은 정확히 하나의 "->"를 포함해야 한다.
   * 
   * - 키와 값은 각각 유효한 type key, MIME type 형식이어야 한다.
   * 
   * - 함수 자체는 인스턴스에 의존하지 않으나, 내부 인스턴스 함수들의 의존한다.
   */
  bool parse_type_mapping(const std::string &line,
                          std::vector<std::string> &keys_out,
                          std::string &value_out);
  /**
   * @brief type 키 문자열의 문법을 검사하고 확장자 목록으로 분리하는 함수
   * @param key 파싱할 키 문자열
   * @return 유효하면 '|'를 기준으로 분리된 키 목록, 그렇지 않으면 빈 벡터
   *
   * - 각 키는 공백을 포함할 수 없으며, '_'와 중복된 확장자는 허용하지 않는다.
   */
  std::vector<std::string> parse_type_keys(const std::string &key);
  /**
   * @brief 문자열이 "type/subtype" 형식의 MIME type 문법에 맞는지 검사하는 함수
   * @param value 검사할 문자열
   * @return MIME type 문법에 맞으면 true, 그렇지 않으면 false
   *
   * - 값은 정확히 하나의 '/'를 포함해야 한다.
   * 
   * - type과 subtype은 비어 있을 수 없다.
   */
  bool is_valid_mime_type(const std::string &value);
  /**
   * @brief 문자열이 server 블록 시작 줄의 형식에 맞는지 검사하는 함수
   * @param line 검사할 문자열
   * @return server 블록 시작 줄 형식이면 true, 그렇지 않으면 false
   *
   * - 문자열은 ':'로 시작해야 하며, 그 뒤에는 하나 이상의 숫자로 이루어진 포트
   * 번호가 와야 한다.
   * 
   * - 포트 번호 뒤에는 선택적으로 하나의 공백이 올 수 있고,
   * 마지막에는 '='가 와야 한다.
   */
  static bool is_server_config_header(const std::string &line);
  /**
   * @brief server 블록 시작 줄에서 포트 번호를 추출하고 ServerConfig 객체를
   * 생성하여 저장하는 함수
   * @param file 파싱할 설정 파일
   * @param line server 블록의 시작 줄
   * @return 저장에 성공하면 true, 실패하면 false
   *
   * - 이미 같은 포트 번호가 등록되어 있으면 실패한다.
   * 
   * - 파싱 중 오류가 발생하면 err_meg에 오류 메시지를 저장한다.
   */
  bool parse_server_config_entry(FileDescriptor &file, const std::string &line);
  /**
   * @brief ":<포트번호> =" 형식의 문자열에서 포트 번호를 파싱하는 함수
   * @param line 포트 번호를 추출할 server 설정 문자열
   * @return 추출한 포트 번호
   *
   * - 입력 문자열은 사전에 server 설정 헤더 문법 검사를 통과한 문자열이어야 한다.
   */
  static unsigned int parse_server_port(const std::string &line);

  WebserverConfig(FileDescriptor &file);

public:
  WebserverConfig &operator=(const WebserverConfig &other) {
    if (this != &other) {
      this->err_meg = other.err_meg;
      this->default_mime = other.default_mime;
      this->uwsgi = other.uwsgi;
      this->type_map = other.type_map;
      this->serverconfig_map = other.serverconfig_map;
    }
    return *this;
  }

  const std::string &get_default_mime(void) const { return default_mime; }
  const std::map<std::string, std::string> &get_uwsgi(void) const {
    return uwsgi;
  }
  const std::map<std::string, std::string> &get_type_map(void) const {
    return type_map;
  }
  const std::map<unsigned int, ServerConfig> &get_serverconfig_map(void) const {
    return serverconfig_map;
  }
  const std::map<int, std::string> &get_default_err_page(void) const { return default_err_page; }
  /**
   * @brief 설정 파일을 파싱한 결과를 Result<WebserverConfig> 형태로 반환하는
   * 함수
   * @param file 파싱할 설정 파일
   * @return 파싱이 성공하면 객체를, 실패하면 오류 메시지를 담은 Result
   *
   * - 일반적인 방식으로 직접 생성할 수 없는 WebserverConfig 객체를
   * 정적 함수 호출을 통해 획득할 수 있도록 제공한다.
   */
  static Result<WebserverConfig> parse(FileDescriptor &file) {
    WebserverConfig temp(file);
    
    if (temp.err_meg == "")
      return OK(WebserverConfig, temp);
    return ERR(WebserverConfig, temp.err_meg);
  }
  /**
   * @brief 기본 에러 페이지 설정 한 줄을 파싱하고 설정에 반영한다.
   * @param line 설정 파일에서 읽은 default error page 관련 한 줄
   * @return 성공 시 빈 문자열, 실패 시 에러 메시지
   *
   * 입력 문자열을 해석하여 정적 에러 페이지 경로 또는
   * CGI 기반 에러 처리 정보를 `default_err_page`에 저장한다.
   *
   * - `key:value` 형식이면 정적 에러 페이지로 처리한다.
   * 
   * - `$...` 형식이면 CGI 실행 정보로 처리한다.
   */
  static std::string apply_default_err_page_entry(const std::string &line, std::map<int, std::string>& err_map);
};

std::ostream &operator<<(std::ostream &os, const WebserverConfig &data);

#endif
