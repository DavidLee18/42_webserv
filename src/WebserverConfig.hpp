#ifndef WEBSERVERCONFIG_HPP
#define WEBSERVERCONFIG_HPP

#include "ServerConfig.hpp"
#include <iosfwd>

/**
 * @class WebserverConfig
 * @brief 웹서버 설정 파일을 파싱하고 그 결과를 멤버 변수에 저장하는 클래스
 *
 * 설정 파일에서 Server, Type, Uwsgi 등의 항목을 읽어들인 뒤
 * 각 설정값을 내부 멤버 변수에 저장하고, 이후 웹서버가 해당
 * 설정 정보를 사용할 수 있도록 제공한다.
 *
 * 이 클래스는 일반적인 방식으로 직접 인스턴스화할 수 없으며,
 * 정적 함수 parse(FileDescriptor &file)가 반환하는
 * Result<WebserverConfig> 를 통해 객체를 획득할 수 있다.
 * 획득한 객체는 복사 또는 대입하여 사용할 수 있다.
 */
class WebserverConfig {
private:
  /**
   * @var err_meg
   * @brief 파싱 중 발생한 오류 메시지를 저장하는 멤버 변수
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
   * 포트 번호를 키로 하고, 실행 파일의 경로를 값으로 저장한다.
   */
  std::map<std::string, std::string> uwsgi;
  /**
   * @var type_map
   * @brief 파일 확장자와 MIME type의 매핑 정보를 저장하는 멤버 변수
   *
   * 파일 확장자를 키로 하고, 해당 확장자에 대응하는 MIME type을 값으로
   * 저장한다. 매핑되지 않은 확장자에 대해서는 멤버 변수 default_mime에 저장된
   * 기본 MIME type을 사용한다.
   */
  std::map<std::string, std::string> type_map;
  /**
   * @var serverconfig_map
   * @brief  설정 파일에서 파싱한 server 설정 정보를 저장하는 멤버 변수
   *
   * 포트 번호를 키로 하고,
   * 해당 포트에 대응하는 server 설정 정보를 값으로 저장한다.
   */
  std::map<unsigned int, ServerConfig> serverconfig_map;

  bool file_parsing(FileDescriptor &file);
  bool set_type_map(FileDescriptor &file);
  bool parse_type_line(const std::string &line,
                       std::vector<std::string> &keys_out,
                       std::string &value_out);
  std::vector<std::string> is_type_key(const std::string &key);
  bool is_type_value(const std::string &value);
  bool is_serverconfig(const std::string &line);
  bool set_serverconfig_map(FileDescriptor &file, const std::string &line);
  unsigned int parse_serverconfig_key(std::string &key);

  WebserverConfig(FileDescriptor &file);

public:
  WebserverConfig(const WebserverConfig &other)
      : default_mime(other.default_mime), uwsgi(other.uwsgi),
        type_map(other.type_map), serverconfig_map(other.serverconfig_map){};

  WebserverConfig &operator=(const WebserverConfig &other) {
    if (this != &other) {
      this->default_mime = other.default_mime;
      this->uwsgi = other.uwsgi;
      this->type_map = other.type_map;
      this->serverconfig_map = other.serverconfig_map;
      this->err_meg.clear();
    }
    return *this;
  }

  const std::string &Get_default_mime(void) const { return default_mime; }
  const std::map<std::string, std::string> &get_uwsgi(void) const {
    return uwsgi;
  }
  const std::map<std::string, std::string> &get_type_map(void) const {
    return type_map;
  }
  const std::map<unsigned int, ServerConfig> &get_serverconfig_map(void) const {
    return serverconfig_map;
  }
  static Result<WebserverConfig> parse(FileDescriptor &file) {
    WebserverConfig temp(file);
    // OK
    if (temp.err_meg == "")
      return OK(WebserverConfig, temp);
    return ERR(WebserverConfig, temp.err_meg);
  }
};

std::ostream &operator<<(std::ostream &os, const WebserverConfig &data);

#endif
