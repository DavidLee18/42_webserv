#ifndef DEFAULTERROR_HPP
#define DEFAULTERROR_HPP

#include "Response.hpp"

class DefaultError {
  virtual int phantom() = 0;

public:
  static Response::StatusCode int_to_status_code(unsigned short status_code);
  static std::string status_code_to_string(Response::StatusCode status_code);
  static Response default_err_response(Response::StatusCode err_code);
};

#endif
