#include "Session.hpp"
#include "../core/Errors.hpp"
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

Result<std::string> Session::generate_session_id() {
  unsigned char random_bytes[16];

  std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
  if (!urandom.is_open()) {
    return ERR(std::string,
               "cannot open /dev/urandom for session ID generation");
  }
  urandom.read(reinterpret_cast<char *>(random_bytes), 16);
  if (urandom.fail() || urandom.gcount() != 16) {
    return ERR(std::string, "cannot read from /dev/urandom");
  }
  urandom.close();

  random_bytes[6] = (random_bytes[6] & 0x0f) | 0x40;
  random_bytes[8] = (random_bytes[8] & 0x3f) | 0x80;

  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  for (int i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      ss << "-";
    ss << std::setw(2) << static_cast<int>(random_bytes[i]);
  }

  return OK(std::string, ss.str());
}

Result<std::string> Session::create_session(const std::string &user_id,
                                            const std::string &client_ip) {
  std::string session_id;
  TRY(std::string, std::string, session_id, generate_session_id())

  SessionData new_session;
  new_session.user_id = user_id;
  new_session.client_ip = client_ip;
  new_session.created_at = std::time(NULL);
  new_session.last_access = new_session.created_at;

  data[session_id] = new_session;
  return OK(std::string, session_id);
}

SessionData* Session::get_session(const std::string& session_id) {
    const std::map<std::string, SessionData>::iterator it =
        data.find(session_id);
    if (it != data.end()) {
        it->second.last_access = std::time(NULL); // 갱신
        return &(it->second);
    }
    return NULL;
}

void Session::delete_session(const std::string& session_id) {
    data.erase(session_id);
}

void Session::clean_expired_sessions(const int timeout_seconds) {
    const time_t                                 now = std::time(NULL);
    std::map<std::string, SessionData>::iterator it  = data.begin();
    while (it != data.end()) {
        if (now - it->second.last_access > timeout_seconds) {
            const std::map<std::string, SessionData>::iterator to_erase = it;
            ++it;
            data.erase(to_erase);
        } else {
            ++it;
        }
    }
}

bool Session::get_session_info(const std::string& session_id,
                               const int timeout_seconds, std::string& user_id,
                               int& elapsed_seconds, int& remaining_seconds) {
    const std::map<std::string, SessionData>::iterator it =
        data.find(session_id);
    if (it == data.end()) return false;

    const time_t now = std::time(NULL);
    elapsed_seconds  = static_cast<int>(now - it->second.created_at);
    remaining_seconds =
        timeout_seconds - static_cast<int>(now - it->second.last_access);

    if (remaining_seconds < 0) remaining_seconds = 0;

    user_id = it->second.user_id;
    return true;
}
