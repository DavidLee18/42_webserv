#ifndef SESSION_HPP
#define SESSION_HPP

#include "../core/Result.hpp"

#include <ctime>
#include <map>
#include <string>

#define TIMEOUT_SECONDS 300

struct SessionData {
    std::string user_id; ///< login user id
    std::string client_ip;

    time_t      created_at;
    time_t      last_access;

    SessionData() : created_at(0), last_access(0) {}
};

class Session {
  std::map<std::string, SessionData> data;
  static Result<std::string> generate_session_id();

public:
  Result<std::string> create_session(const std::string &user_id,
                                     const std::string &client_ip);
  SessionData *get_session(const std::string &session_id);
  void delete_session(const std::string &session_id);
  void clean_expired_sessions(int timeout_seconds);
  bool get_session_info(const std::string &session_id, int timeout_seconds,
                        std::string &user_id, int &elapsed_seconds,
                        int &remaining_seconds);
};

#endif