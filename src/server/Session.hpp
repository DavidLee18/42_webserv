#ifndef SESSION_HPP
#define SESSION_HPP

#include <ctime>
#include <string>

class Session {
private:
  std::string session_id;   // random value generated
  std::string user_id;      // login user id
  std::string client_ip;

  time_t created_at;
  time_t last_access;

public:
  Session(std::string cookie);
  ~Session();
};

#endif