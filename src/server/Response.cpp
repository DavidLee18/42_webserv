#include "Response.hpp"
#include "Session.hpp"
#include "../cgi_1_1.h"
#include "ParsingUtils.hpp"

std::string get_string_from_map(const std::map<int, std::string> map, int key) {
  std::map<int, std::string>::const_iterator it = map.find(key);

  if (it != map.end())
    return it->second;
  else
    return "";
}

std::string get_string_from_map(const std::map<std::string, std::string> map,
                                std::string key) {
  std::map<std::string, std::string>::const_iterator it = map.find(key);

  if (it != map.end())
    return it->second;
  else
    return "";
}

std::string ServerResponse::status_code_to_string(int status_code) {
  if (status_code == 200)
    return "200 OK";
  else if (status_code == 301)
    return "301 Moved Permanently";
  else if (status_code == 400)
    return "400 Bad Request";
  else if (status_code == 403)
    return "403 Forbidden";
  else if (status_code == 404)
    return "404 Not Found";
  else if (status_code == 405)
    return "405 Method Not Allowed";
  else if (status_code == 413)
    return "413 Payload Too Large";
  else if (status_code == 500)
    return "500 Internal Server Error";
  return "500 Internal Server Error";
}

std::string ServerResponse::get_pwd() {
  char buffer[1024];
  if (getcwd(buffer, sizeof(buffer)) != NULL) {
    return std::string(buffer);
  }
  return "";
}

int ServerResponse::check_path_type(const std::string &path) {
  struct stat info;

  if (stat(path.c_str(), &info) != 0)
    return NOT_FOUND_ERR;
  else if (access(path.c_str(), R_OK) != 0)
    return FORBIDDEN_ERR;
  else if (S_ISDIR(info.st_mode))
    return IS_DIR;
  else if (S_ISREG(info.st_mode))
    return IS_FILE;
  return PATH_ERROR;
}

std::string ServerResponse::find_file_type(std::string path) {
  std::vector<std::string> file_type = utils::string_split(path, ".");

  if (file_type.size() <= 1)
    return "default";
  return file_type.back();
}

Target ServerResponse::resolve_target(const RouteRule *rule,
                                      const ServerConfig *config,
                                      const Request *request) {
  Target target;
  if (rule == NULL) {
    target.type = NOT_FOUND_ERR;
    return target;
  }

  std::string root =
      config->get_rewritten_path(request->get_method(), request->get_path());

  target.path = get_pwd();
  std::cout << "Root: " << root << std::endl;
  std::cout << "rule op: " << rule->op << std::endl;
  int type = check_path_type(target.path + root);
  if (type == IS_DIR) {
    target.path += root;
    if (rule->op == SERVEFROM && request->get_path() == "/")
      target.path += rule->index;
  } else if (type == NOT_FOUND_ERR)
    target.path += get_string_from_map(rule->error_pages, NOT_FOUND_ERR);
  else if (type == FORBIDDEN_ERR)
    target.path += get_string_from_map(rule->error_pages, FORBIDDEN_ERR);
  else
    target.path += root;
  target.type = check_path_type(target.path);

  std::cout << "target path: " << target.path << std::endl;
  std::cout << "rule index: " << rule->index << std::endl;
  return target;
}

Response ServerResponse::error_response(const ServerConfig *config,
                                        const RouteRule *rule, int err_code) {
  Response response;
  std::string err_page = get_pwd() + get_string_from_map(rule->error_pages, err_code);
  std::cout << "error page: " << err_page << std::endl;

  if (err_page.empty())
    return DefaultError::default_err_response(err_code);
  (void)config;
  if (check_path_type(err_page) != IS_FILE)
    return DefaultError::default_err_response(err_code);
  std::ifstream file(err_page.c_str());
  if (file.is_open()) {
    response.status_code = status_code_to_string(OK);
    std::ostringstream ss;
    ss << file.rdbuf();
    response.body = ss.str();
    file.close();
  } else {
    return DefaultError::default_err_response(err_code);
  }
  return response;
}

Response ServerResponse::delete_method(Target target, Response response,
                                       const ServerConfig *config,
                                       const RouteRule *rule) {
  if (unlink(target.path.c_str()) == 0) {
    response.status_code = "204 No Content";
    return response;
  } else {
    return error_response(config, rule, FORBIDDEN_ERR); // 404, 500
  }
}

Response ServerResponse::post_method(Target target, Response response,
                                     const ClientSession *client,
                                     const RouteRule *rule,
                                     const Request *request,
                                     Session *session) {
  (void)target;
  const ServerConfig *config = client->config;
  if (request->get_path() == "/login" || request->get_path() == "/login.html") {
    std::string body = request->get_body();
    std::string id = "";
    std::string pw = "";

    std::string auth_target = get_pwd() +
        config->get_rewritten_path(request->get_method(), rule->auth_info);
    std::cout << "\n" << auth_target << "\n" << std::endl;
    std::ifstream file(auth_target.c_str());
    if (file.is_open()) {
      std::map<std::string, std::string> auth_info;
      size_t id_pos = body.find("id=");
      if (id_pos != std::string::npos) {
        size_t amp_pos = body.find('&', id_pos);
        if (amp_pos == std::string::npos)
          amp_pos = body.length();
        id = body.substr(id_pos + 3, amp_pos - (id_pos + 3));
      }
      size_t pw_pos = body.find("pw=");
      if (pw_pos != std::string::npos) {
        size_t amp_pos = body.find('&', pw_pos);
        if (amp_pos == std::string::npos)
          amp_pos = body.length();
        pw = body.substr(pw_pos + 3, amp_pos - (pw_pos + 3));
      }

      std::string line;
      bool is_authenticated = false;
      std::string target_credential = id + ":" + pw;

      while (std::getline(file, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r') {
          line = line.substr(0, line.length() - 1);
        }
        if (line == target_credential) {
          is_authenticated = true;
          break;
        }
      }
      file.close();

      if (is_authenticated) {
        std::cout << "Authentication SUCCESS for: " << id << std::endl;
        
        // 1. 브라우저에게 "이 주소로 가라"고 알리는 상태 코드 설정
        // 일반적으로 다른 페이지 이동 시 302 혹은 303을 사용
        response.status_code = "302 Found"; 
        response.redir = rule->index;
        response.mime_type = "text/html";
        response.body = "<html><body>Redirecting...</body></html>";
        response.cookie = "session_id=" + session->create_session(id, client->ip) + "; Path=/; HttpOnly";
        return response;
      } else {
        std::cout << "Authentication FAILED for: " << id << std::endl;
        
        response.status_code = status_code_to_string(200);
        response.mime_type = "text/html";
        // 로그인 실패 시 브라우저 자체 Alert 팝업을 띄우고 이전(로그인) 화면으로 다시 돌려보냅니다.
        response.body = "<script>"
                        "alert('invalid id or password');"
                        "window.location.href='/login.html';"
                        "</script>";
        return response;
      }
    } else
      return error_response(config, rule, NOT_FOUND_ERR);
  }
  return error_response(config, rule, FORBIDDEN_ERR);
}

Response ServerResponse::get_method(Target target, Response response,
                                    const ServerConfig *config,
                                    const RouteRule *rule,
                                    const Request *request) {
  if (rule->op == REDIRECT) {
    std::cout << "=== redirection ===" << std::endl;
    target.type = MOVED_PERMANENTLY;
    response.redir =
        config->get_rewritten_path(request->get_method(), request->get_path());
    response.status_code = status_code_to_string(target.type);
    response.mime_type = "text/html";
    response.body = "<html><body><h1>301 Moved Permanently</h1></body></html>";
  } else if (target.type == IS_DIR && rule->op == AUTOINDEX) {
    DIR *dir = opendir(target.path.c_str());
    if (dir == NULL) {
      return error_response(
          config, rule,
          FORBIDDEN_ERR); // TODO: 폴더를 열 권한이 없거나 없으면 빈 문자열
                          // 반환 (나중에 403처리)
    }
    target.type = OK;
    response.mime_type = "html";
    response.body = make_autoindex_page(target.path, request->get_path(), dir);
    response.status_code = status_code_to_string(target.type);
  } else {
    if (check_path_type(target.path.c_str()) != IS_FILE)
      return DefaultError::default_err_response(NOT_FOUND_ERR);
    std::ifstream file(target.path.c_str());
    if (file.is_open()) {
      std::ostringstream ss;
      ss << file.rdbuf();
      target.type = OK;
      response.body = ss.str();
      response.status_code = status_code_to_string(target.type);
      file.close();
    } else {
      return DefaultError::default_err_response(NOT_FOUND_ERR);
    }
  }
  return response;
}

Response ServerResponse::http_response(
    const Request *request, const ClientSession *client,
    const std::map<std::string, std::string> mime_type,
    Session *session) {
  const ServerConfig *config = client->config;
  const RouteRule *rule =
      config->find_route(request->get_method(), request->get_path());
  Response response;
  Target target = resolve_target(rule, config, request);

  // [쿠키 검증 로직 추가]
  std::string session_id = request->get_cookie_value("session_id");
  SessionData* user_session = NULL;
  
  if (!session_id.empty()) {
    user_session = session->get_session(session_id);
  }

  if (user_session) {
    std::cout << "[Authentication] Valid user session found! User ID: " << user_session->user_id << std::endl;
  } else {
    // 세션이 없는데 보호된 자원(예: DELETE 명령)을 요청하면 401 에러를 반환
    if (request->get_method() == Request::DELETE) {
      std::cout << "[Authentication] Blocked DELETE request. No valid session." << std::endl;
      return error_response(config, rule, UNAUTHORIZED);
    }
    std::cout << "[Authentication] No valid session. Guest user." << std::endl;
  }

  response.mime_type =
      get_string_from_map(mime_type, find_file_type(target.path));
  std::cout << "mime type: " << response.mime_type << std::endl;

  if (request->get_method() == Request::DELETE) {
    return ServerResponse::delete_method(target, response, config, rule);
  } else if (request->get_method() == Request::POST) {
    return ServerResponse::post_method(target, response, client, rule, request, session);
  } else if (request->get_method() == Request::GET) {
    return ServerResponse::get_method(target, response, config, rule, request);
  } else {
    return error_response(config, rule, METHOD_NOT_ALLOWED);
  }
  return response;
}

Response ServerResponse::cgi_response(const Request *request,
                                      const ServerConfig *config,
                                      EPoll *epoll) {
  Response response;
  CgiDelegate cgi(*request, get_pwd() + request->get_path());
  Result<std::string> cgi_result =
      cgi.execute(config->get_server_response_time(), epoll);
  if (!cgi_result.has_value()) {
    std::cerr << "CGI ERROR: " << cgi_result.error() << std::endl;
    response.status_code = status_code_to_string(500);
    response.mime_type = "text/plain";
    response.body = "Internal Server Error: " + cgi_result.error();
    return response;
  }

  std::string out = cgi_result.value();
  std::string headers_section;
  std::string body_section;
  size_t blank_line_pos = out.find("\r\n\r\n");

  if (blank_line_pos == std::string::npos) {
    blank_line_pos = out.find("\n\n");
    if (blank_line_pos != std::string::npos) {
      headers_section = out.substr(0, blank_line_pos);
      body_section = out.substr(blank_line_pos + 2);
    } else {
      body_section = out;
    }
  } else {
    headers_section = out.substr(0, blank_line_pos);
    body_section = out.substr(blank_line_pos + 4);
  }

  std::string status = "200 OK";
  size_t status_pos = headers_section.find("Status: ");
  if (status_pos != std::string::npos) {
    size_t end = headers_section.find("\n", status_pos);
    if (end != std::string::npos) {
      status = headers_section.substr(status_pos + 8, end - status_pos - 8);
      if (!status.empty() && status[status.length() - 1] == '\r')
        status = status.substr(0, status.length() - 1);
    } else {
      status = headers_section.substr(status_pos + 8);
    }
  }

  std::ostringstream full_resp;
  full_resp << "HTTP/1.1 " << status << "\r\n";
  if (!headers_section.empty())
    full_resp << headers_section << "\r\n";
  full_resp << "Content-Length: " << body_section.length() << "\r\n\r\n";
  full_resp << body_section;

  response.cgi = full_resp.str();
  return response;
}

std::string ServerResponse::make_autoindex_page(const std::string &real_path,
                                                const std::string &req_uri,
                                                DIR *dir) {
  std::ostringstream html;

  // 1. HTML 기본 뼈대 및 모던 다크 테마 CSS 작성
  html
      << "<!DOCTYPE html>\n"
      << "<html><head><meta charset=\"UTF-8\">\n"
      << "<meta name=\"viewport\" content=\"width=device-width, "
         "initial-scale=1.0\">\n"
      << "<title>Index of " << req_uri << "</title>\n"
      << "<style>\n"
      << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', "
         "Roboto, Helvetica, Arial, sans-serif; background-color: #0d1117; "
         "color: #c9d1d9; margin: 0; padding: 40px 20px; }\n"
      << ".container { max-width: 800px; margin: 0 auto; background: #161b22; "
         "border: 1px solid #30363d; border-radius: 12px; box-shadow: 0 8px "
         "24px rgba(0,0,0,0.2); overflow: hidden; }\n"
      << ".header { padding: 20px 24px; border-bottom: 1px solid #30363d; "
         "background: #21262d; }\n"
      << "h1 { margin: 0; font-size: 18px; font-weight: 600; word-break: "
         "break-all; color: #8b949e; }\n"
      << "h1 span { color: #e6edf3; }\n"
      << ".list { list-style: none; padding: 0; margin: 0; }\n"
      << ".item { border-bottom: 1px solid #21262d; }\n"
      << ".item:last-child { border-bottom: none; }\n"
      << ".link { display: flex; align-items: center; padding: 14px 24px; "
         "text-decoration: none; color: #58a6ff; transition: all 0.2s ease; }\n"
      << ".link:hover { background-color: #30363d; transform: translateX(4px); "
         "}\n"
      << ".icon { margin-right: 14px; font-size: 20px; width: 24px; "
         "text-align: center; }\n"
      << "</style></head><body>\n"
      << "<div class=\"container\">\n"
      << "  <div class=\"header\">\n"
      << "    <h1>Index of <span>" << req_uri << "</span></h1>\n"
      << "  </div>\n"
      << "  <ul class=\"list\">\n";

  struct dirent *entity;

  // 2. 디렉토리 안의 파일들을 하나씩 읽기
  while ((entity = readdir(dir)) != NULL) {
    std::string name = entity->d_name;

    // 현재 폴더(.)는 굳이 보여줄 필요가 없으니 스킵
    if (name == ".")
      continue;

    // 절대 경로를 합쳐서 진짜 폴더인지 검사
    std::string full_item_path = real_path + name;
    int type = check_path_type(full_item_path);

    std::string icon = "📄"; // 기본 파일 아이콘

    if (type == IS_DIR) {
      name += "/";
      icon = "📁"; // 폴더 아이콘
    }

    if (name == "../") {
      icon = "🔙"; // 상위 폴더 아이콘
    }

    html << "    <li class=\"item\"><a href=\"" << name
         << "\" class=\"link\">\n"
         << "      <span class=\"icon\">" << icon << "</span>\n"
         << "      <span>" << name << "</span>\n"
         << "    </a></li>\n";
  }

  html << "  </ul>\n"
       << "</div>\n"
       << "</body></html>";

  closedir(dir);

  return html.str();
}
