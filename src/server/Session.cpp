#include "Session.hpp"
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <iomanip>

std::string Session::generate_session_id() {
    unsigned char random_bytes[16];
    bool urandom_success = false;

    // 1. /dev/urandom에서 난수 추출 시도 (가장 안전한 방법)
    std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
    if (urandom.is_open()) {
        urandom.read(reinterpret_cast<char*>(random_bytes), 16);
        if (urandom) {
            urandom_success = true;
        }
        urandom.close();
    }

    // 2. 실패 시 rand()로 fallback
    if (!urandom_success) {
        std::srand(static_cast<unsigned int>(std::time(NULL)));
        for (int i = 0; i < 16; ++i) {
            random_bytes[i] = static_cast<unsigned char>(std::rand() % 256);
        }
    }

    // 3. UUID v4 규칙 적용
    random_bytes[6] = (random_bytes[6] & 0x0f) | 0x40; // M 위치: 4 (0100)
    random_bytes[8] = (random_bytes[8] & 0x3f) | 0x80; // N 위치: 8, 9, a, b (10xx)

    // 4. 16진수 문자열 포맷팅
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            ss << "-";
        ss << std::setw(2) << static_cast<int>(random_bytes[i]);
    }

    return ss.str();
}

std::string Session::create_session(const std::string& user_id, const std::string& client_ip) {
    std::string session_id = generate_session_id();
    SessionData new_session;
    new_session.user_id = user_id;
    new_session.client_ip = client_ip;
    new_session.created_at = std::time(NULL);
    new_session.last_access = new_session.created_at;

    data[session_id] = new_session;
    return session_id;
}

SessionData* Session::get_session(const std::string& session_id) {
  const std::map<std::string, SessionData>::iterator it = data.find(session_id);
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
  const time_t now = std::time(NULL);
    std::map<std::string, SessionData>::iterator it = data.begin();
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



