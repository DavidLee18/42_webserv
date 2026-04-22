#ifndef DEFAULTERROR_HPP
#define DEFAULTERROR_HPP

#include <string>

struct Response;

class DefaultError {
private:
  virtual int phantom() = 0;
  static std::string bad_request();
  static std::string forbidden();
  static std::string not_found();
  static std::string server_error();
  static std::string unknown_err();
  static std::string status_code_to_string(int status_code);

public:
  static Response default_err_response(int err_code);
};

#endif
