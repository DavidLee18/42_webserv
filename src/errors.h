#ifndef ERRORS_H
#define ERRORS_H

#include <string>

class Errors {
  virtual void phantom() = 0;

public:
  const static std::string invalid_fd;
  const static std::string iter_ended;
  const static std::string interrupted;
  const static std::string fd_not_registered;
  const static std::string invalid_operation;
  const static std::string epoll_loop;
  const static std::string out_of_mem;
  const static std::string epoll_full;
  const static std::string not_supported;
  const static std::string access_denied;
  const static std::string fd_too_many;
  const static std::string addr_not_available;
  const static std::string address_fault;
  const static std::string addr_loop;
  const static std::string name_too_long;
  const static std::string not_found;
  const static std::string readonly_filesys;
  const static std::string try_again;
  const static std::string conn_aborted;
  const static std::string unknown_char;
  const static std::string invalid_format;
  const static std::string too_long_num;
  const static std::string out_of_rng;
  const static std::string invalid_json;
  const static std::string stream_too_many;
};

#endif // ERRORS_H
