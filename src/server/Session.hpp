#ifndef SESSION_HPP
#define SESSION_HPP

#include <ctime>
#include <string>
#include <map>

struct SessionData
{
  std::string user_id;    ///< login user id
  std::string client_ip;

  time_t created_at;
  time_t last_access;
};

/**
 * @class Session
 * @brief Session class to generate uuid and save data.
 */
class Session {
  /**
   * @brief Map tying session IDs to their specific session data.
   * Key: Session ID. Value: Session data.
   */
  std::map<std::string, SessionData> data;

  static std::string generate_session_id();

public:

  // 세션을 생성하고, 생성된 고유 Session ID를 반환
  std::string create_session(const std::string& user_id, const std::string& client_ip);
  
  // 세션이 유효한지 확인하고 데이터를 받아옴 (없거나 만료시 NULL 반환)
  SessionData* get_session(const std::string& session_id);
  
  // 로그아웃 시 세션 명시적 삭제
  void delete_session(const std::string& session_id);
  
  // 일정 시간(timeout_seconds) 동안 활동이 없는 좀비 세션을 삭제
  void clean_expired_sessions(int timeout_seconds);

  // 세션 정보 반환 (JSON 형식으로 사용 가능)
  bool get_session_info(const std::string& session_id, int timeout_seconds,
                        std::string& user_id, int& elapsed_seconds, int& remaining_seconds);
};

#endif