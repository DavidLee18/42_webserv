#include "Response.hpp"
#include "../cgi_1_1.h"
#include "../utils.hpp"
#include "Session.hpp"

#include <algorithm>
#include <cerrno>
#include <ctime>
#include <sys/stat.h>

std::string get_string_from_map(const std::map<unsigned int, std::string> &map,
                                const int key) {
  const std::map<unsigned int, std::string>::const_iterator it =
      map.find(static_cast<unsigned int>(key));

  if (it != map.end())
    return it->second;
  else
    return "";
}

std::string get_string_from_map(const std::map<std::string, std::string> &map,
                                const std::string &key) {
  const std::map<std::string, std::string>::const_iterator it = map.find(key);

  if (it != map.end())
    return it->second;
  else
    return "";
}

static std::string get_http_date() {
  const time_t now = std::time(NULL);
  const struct tm *timeinfo = std::gmtime(&now);
  char buffer[100];
  std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
  return std::string(buffer);
}

std::ostream &operator<<(std::ostream &os, Response const &resp) {
  os << "HTTP/1.1 " << DefaultError::status_code_to_string(resp.status_code)
     << utils::crlf;
  os << "Date: " << get_http_date() << utils::crlf;
  os << "Server: webserv" << utils::crlf;
  if ((resp.status_code == Response::MOVED_PERMANENTLY ||
       resp.status_code == Response::FOUND) &&
      !resp.redir.empty())
    os << "Location: " << resp.redir << utils::crlf;
  os << "Content-Type: " << resp.content_type << utils::crlf;
  if (!resp.cookie.empty())
    os << "Set-Cookie:" << resp.cookie << utils::crlf;
  for (std::map<std::string, std::string>::const_iterator it =
           resp.headers.begin();
       it != resp.headers.end(); ++it)
    os << it->first << ": " << it->second << utils::crlf;
  os << "Content-Length: " << resp.content_length << utils::crlf;
  if (resp.keep_alive)
    os << "Connection: keep-alive" << utils::crlf;
  else
    os << "Connection: close" << utils::crlf;
  os << utils::crlf;
  if (!resp.body.empty())
    os << resp.body;
  return os;
}

void Response::print_simple(std::ostream &os) const {
  os << utils::info << "HTTP/1.1 "
     << DefaultError::status_code_to_string(status_code) << utils::crlf;
  os << utils::info << "Date: " << get_http_date() << utils::crlf;
  os << utils::info << "Server: webserv" << utils::crlf;
  if ((status_code == Response::MOVED_PERMANENTLY ||
       status_code == Response::FOUND) &&
      !redir.empty())
    os << utils::info << "Location: " << redir << utils::crlf;
  os << utils::info << "Content-Type: " << content_type << utils::crlf;
  if (!cookie.empty())
    os << utils::info << "Set-Cookie:" << cookie << utils::crlf;
  for (std::map<std::string, std::string>::const_iterator it = headers.begin();
       it != headers.end(); ++it)
    os << utils::info << it->first << ": " << it->second << utils::crlf;
  os << utils::info << "Content-Length: " << content_length << utils::crlf;
  if (keep_alive)
    os << utils::info << "Connection: keep-alive" << utils::crlf;
  else
    os << utils::info << "Connection: close" << utils::crlf;
  os << utils::crlf;
}

std::string ServerResponse::find_file_type(const std::string &path) {
  std::vector<std::string> file_type = utils::string_split(path, ".");

  if (file_type.size() <= 1)
    return "default";
  return file_type.back();
}

std::string
ServerResponse::get_mime_type_for_extension(const std::string &ext) {
  if (ext == "html" || ext == "htm")
    return "text/html";
  else if (ext == "json")
    return "application/json";
  else if (ext == "txt")
    return "text/plain";
  else if (ext == "xml")
    return "application/xml";
  else if (ext == "css")
    return "text/css";
  else if (ext == "js")
    return "application/javascript";
  else if (ext == "jpg" || ext == "jpeg")
    return "image/jpeg";
  else if (ext == "png")
    return "image/png";
  else if (ext == "gif")
    return "image/gif";
  return "text/html"; // default
}

Response ServerResponse::http_response(
    const Request *request, const ClientSession *client,
    const std::map<std::string, std::string> &mime_type, Session *session,
    char **envp) {
  const ServerConfig *config = client->config;
  if (config == NULL)
    return DefaultError::default_err_response(Response::INTERNAL_SERVER_ERR);
  const RouteRule *rule =
      config->find_route(request->get_method(), request->get_path());
  Response response;
  if (rule == NULL) {
    response = DefaultError::default_err_response(Response::NOT_FOUND);
    response.headers = config->get_header();
    return response;
  } else if (request->get_path().find("../") != std::string::npos) {
    std::cout << utils::info << "request path: " << request->get_path()
              << std::endl;
    response = DefaultError::default_err_response(Response::BAD_REQUEST);
    response.headers = config->get_header();
    return response;
  }
  // payload check
  Result<size_t>res_content_len = request->get_content_length();
  if (!res_content_len.has_value())
    return error_response(config, rule, Response::FORBIDDEN, envp);
  size_t content_len = res_content_len.value();
  if (content_len > static_cast<size_t>(rule->max_body_KB * 1000))
    return error_response(config, rule, Response::PAYLOAD_TOO_LARGE, envp);

  response.headers = config->get_header();
  const Target target = resolve_target(rule, config, request, envp);

  // [쿠키 검증 로직 추가]
  const std::string session_id = request->get_cookie_value("session_id");
  const SessionData *user_session = NULL;

  if (!session_id.empty()) {
    user_session = session->get_session(session_id);
  }

  if (user_session) {
    std::cout << utils::info
              << "[Authentication] Valid user session found! User ID: "
              << user_session->user_id << std::endl;
  } else {
    // 세션이 없는데 보호된 자원(예: DELETE 명령)을 요청하면 401 에러를 반환
    if (request->get_method() == Request::DELETE) {
      std::cout << utils::info
                << "[Authentication] Blocked DELETE request. No valid session."
                << std::endl;
      response = error_response(config, rule, Response::UNAUTHORIZED, envp);
      response.headers = config->get_header();
      return response;
    } else {
      std::cout << utils::info
                << "[Authentication] No valid session. Guest user."
                << std::endl;
    }
  }

  switch (request->get_method()) {
  case Request::DELETE:
    response =
        ServerResponse::delete_method(target, response, config, rule, envp);
    break;
  case Request::POST:
    response = ServerResponse::post_method(target, response, client, rule,
                                           request, session, envp);
    break;
  case Request::HEAD:
  case Request::GET:
    response = ServerResponse::get_method(target, response, config, rule,
                                          request, envp);
    break;
  default:
    response = error_response(config, rule, Response::METHOD_NOT_ALLOWED, envp);
    break;
  }

  response.headers = config->get_header();
  response.keep_alive = request->has_keep_alive();
  response.content_length = response.body.length();
  if (request->get_method() == Request::HEAD)
    response.body.clear();

  // Handle conditional requests (If-None-Match, If-Modified-Since)
  if (response.status_code == Response::OK ||
      response.status_code == Response::MOVED_PERMANENTLY ||
      response.status_code == Response::FOUND) {
    // Check If-None-Match (ETag)
    std::string if_none_match = get_string_from_map(request->get_headers(), "If-None-Match");
    if (!if_none_match.empty() && !response.headers["ETag"].empty()) {
      // ETag match ("*" or exact match)
      if (if_none_match == "*" || if_none_match == response.headers["ETag"]) {
        response.status_code = Response::NOT_MODIFIED;
        response.body.clear();
        response.content_length = 0;
        return response;
      }
    }
    // Check If-Modified-Since
    std::string if_modified_since = get_string_from_map(request->get_headers(), "If-Modified-Since");
    if (!if_modified_since.empty() && !response.headers["Last-Modified"].empty()) {
      // Simple comparison: if Last-Modified <= If-Modified-Since, return 304
      if (response.headers["Last-Modified"] <= if_modified_since) {
        response.status_code = Response::NOT_MODIFIED;
        response.body.clear();
        response.content_length = 0;
        return response;
      }
    }
  }

  // Only set content_type from mime_type map if not already set by method handler
  if (response.content_type.empty())
    response.content_type =
        get_string_from_map(mime_type, find_file_type(target.path));
  // Special API endpoint for session info
  if (request->get_path() == "/api/session-info") {
    if (request->get_method() == Request::GET) {
      response.content_type = "application/json";
      response.status_code = Response::OK;

      if (session_id.empty()) {
        response.body =
            "{\"logged_in\":false,\"message\":\"No active session\"}";
        response.content_length = response.body.length();
        return response;
      } else {
        std::string user_id;
        int elapsed_seconds = 0;
        int remaining_seconds = 0;
        const int timeout_seconds = TIMEOUT_SECONDS;

        if (session->get_session_info(session_id, timeout_seconds, user_id,
                                      elapsed_seconds, remaining_seconds)) {
          std::ostringstream json;
          json << "{\"logged_in\":true,\"user_id\":\"" << user_id
               << "\",\"elapsed_seconds\":" << elapsed_seconds
               << ",\"remaining_seconds\":" << remaining_seconds
               << ",\"timeout_seconds\":" << timeout_seconds << "}";
          response.body = json.str();
        } else {
          response.body =
              "{\"logged_in\":false,\"message\":\"Session expired\"}";
        }
        response.content_length = response.body.length();
      }
      return response;
    } else {
      return error_response(config, rule, Response::METHOD_NOT_ALLOWED, envp);
    }
  } else {
    return response;
  }
}

Result<Void> ServerResponse::register_cgi(
    const Request &request, const RouteRule_CGI &rule, EPoll *epoll,
    std::map<std::string, std::string> const &cgi_interpreters,
    std::map<FileDescriptor const *,
             std::pair<FileDescriptor const *, CgiDelegate *> > &cgis,
    FileDescriptor const *client_fd, char **envp) {
  CgiDelegate *del =
      static_cast<CgiDelegate *>(operator new(sizeof(CgiDelegate)));
  const Result<CgiDelegate> del_ =
      CgiDelegate::from_req(request, *epoll, rule, cgi_interpreters, envp);
  if (!del_.has_value()) {
    operator delete(del);
    return ERR(Void, del_.error());
  }
  new (del) CgiDelegate(del_.value());
  Result<Void> res = del->register_(cgis, client_fd);
  if (!res.has_value()) {
    del->~CgiDelegate();
    operator delete(del);
    return ERR(Void, res.error());
  }
  return OKV;
}

int ServerResponse::check_path_type(const std::string &path) {
  struct stat info = {};

  if (stat(path.c_str(), &info) != 0)
    return Response::NOT_FOUND;
  else if (access(path.c_str(), R_OK) != 0)
    return Response::FORBIDDEN;
  else if (S_ISDIR(info.st_mode))
    return IS_DIR;
  else if (S_ISREG(info.st_mode)) {
    if (path.rfind(".cgi") == path.length() - 4) {
      return Response::NOT_FOUND;
    }
    return IS_FILE;
  }
  return PATH_ERROR;
}

Target ServerResponse::resolve_target(const RouteRule *rule,
                                      const ServerConfig *config,
                                      const Request *request, char **envp) {
  Target target;
  if (rule == NULL) {
    target.type = Response::NOT_FOUND;
    return target;
  }

  const std::string root =
      config->get_rewritten_path(request->get_method(), request->get_path());

  target.path = utils::get_env("PWD", envp);
  const int type = check_path_type(target.path + root);
  if (type == IS_DIR) {
    if (rule->op == SERVE_FROM && request->get_path() == "/") {
      target.path += rule->index;
    } else {
      target.path += root;
    }
    target.type = check_path_type(target.path);
  } else if (type == Response::NOT_FOUND) {
    target.path += get_string_from_map(rule->error_pages, Response::NOT_FOUND);
    target.type = Response::NOT_FOUND; // Keep error code, don't check path type
  } else if (type == Response::FORBIDDEN) {
    target.path += get_string_from_map(rule->error_pages, Response::FORBIDDEN);
    target.type = Response::FORBIDDEN; // Keep error code, don't check path type
  } else {
    target.path += root;
    target.type = check_path_type(target.path);
  }

  return target;
}

std::string ServerResponse::get_pwd() {
  char buffer[1024];
  if (getcwd(buffer, sizeof(buffer)) != NULL) {
    return std::string(buffer);
  }
  return "";
}

std::string ServerResponse::compute_etag(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) == -1)
    return "";

  std::ostringstream oss;
  oss << "\"" << st.st_ino << "-" << st.st_size << "-" << st.st_mtime << "\"";
  return oss.str();
}

std::string ServerResponse::get_last_modified(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) == -1)
    return "";

  // Format time as RFC 7231 date
  char buf[100];
  struct tm *tm_info = gmtime(&st.st_mtime);
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
  return std::string(buf);
}

Response ServerResponse::error_response(const ServerConfig *config,
                                        const RouteRule *rule,
                                        const Response::StatusCode error_code,
                                        char **envp) {
  std::string err_page = utils::get_env("PWD", envp) +
                         get_string_from_map(rule->error_pages, error_code);

  if (err_page.empty())
    return DefaultError::default_err_response(error_code);
  if (check_path_type(err_page) != IS_FILE)
    return DefaultError::default_err_response(error_code);
  std::ifstream file(err_page.c_str());
  if (file.is_open()) {
    Response response;
    response.status_code = error_code;
    response.headers = config->get_header();
    // Use get_mime_type_for_extension helper instead of map lookup
    response.content_type =
        get_mime_type_for_extension(find_file_type(err_page));
    std::ostringstream ss;
    ss << file.rdbuf();
    response.body = ss.str();
    response.content_length = response.body.length();
    file.close();
    return response;
  } else {
    return DefaultError::default_err_response(error_code);
  }
}

static std::string escape_html(const std::string &input) {
  std::string escaped;

  escaped.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '&')
      escaped += "&amp;";
    else if (input[i] == '<')
      escaped += "&lt;";
    else if (input[i] == '>')
      escaped += "&gt;";
    else if (input[i] == '"')
      escaped += "&quot;";
    else if (input[i] == '\'')
      escaped += "&#39;";
    else
      escaped += input[i];
  }
  return escaped;
}

std::string ServerResponse::make_autoindex_page(const std::string &real_path,
                                                const std::string &req_uri,
                                                DIR *dir) {
  std::ostringstream html;

  html << "<!DOCTYPE html>\n"
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
       << ".entry { display: flex; align-items: center; gap: 12px; padding: "
          "14px 24px; }\n"
       << ".link { display: flex; align-items: center; flex: 1; min-width: 0; "
          "text-decoration: none; color: #58a6ff; transition: transform 0.2s "
          "ease; }\n"
       << ".link:hover { transform: translateX(4px); }\n"
       << ".icon { margin-right: 14px; font-size: 20px; width: 24px; "
          "text-align: center; flex: 0 0 24px; }\n"
       << ".delete-btn { border: none; border-radius: 8px; padding: 8px 14px; "
          "background: #da3633; color: #fff; font-size: 13px; font-weight: "
          "600; cursor: pointer; transition: transform 0.15s ease, "
          "background-color 0.15s ease; }\n"
       << ".delete-btn:hover { background: #f85149; transform: "
          "translateY(-1px); }\n"
       << ".delete-btn:active { transform: translateY(0); }\n"
       << "</style></head><body>\n"
       << "<div class=\"container\">\n"
       << "  <div class=\"header\">\n"
       << "    <h1>Index of <span>" << req_uri << "</span></h1>\n"
       << "  </div>\n"
       << "  <ul class=\"list\">\n"
       << "  <script src=\"/autoindex-delete.js\" defer></script>\n";

  dirent *entity;
  while ((entity = readdir(dir)) != NULL) {
    std::string name = entity->d_name;
    if (name == ".")
      continue;

    std::string full_item_path = real_path + name;
    const int type = check_path_type(full_item_path);
    std::string icon = "📄";

    if (type == IS_DIR) {
      name += "/";
      icon = "📁";
    }
    if (name == "../")
      icon = "🔙";

    const bool deletable = (type != IS_DIR && name != "../");
    const std::string item_path = req_uri + name;

    html << "    <li class=\"item\">\n"
         << "      <div class=\"entry\">\n"
         << "        <a href=\"" << escape_html(name) << "\" class=\"link\">\n"
         << "          <span class=\"icon\">" << icon << "</span>\n"
         << "          <span>" << escape_html(name) << "</span>\n"
         << "        </a>\n";
    if (deletable) {
      html << "        <button type=\"button\" class=\"delete-btn\" "
              "data-delete-target=\""
           << escape_html(item_path) << "\">Delete</button>\n";
    }
    html << "      </div>\n"
         << "    </li>\n";
  }

  html << "  </ul>\n"
       << "</div>\n"
       << "</body></html>";

  closedir(dir);
  return html.str();
}

std::string ServerResponse::extract_boundary(const std::string &content_type) {
  size_t boundary_pos = content_type.find("boundary=");
  if (boundary_pos == std::string::npos)
    return "";

  boundary_pos += 9; // Length of "boundary="
  size_t end_pos = content_type.find(';', boundary_pos);
  if (end_pos == std::string::npos)
    end_pos = content_type.find('\r', boundary_pos);
  if (end_pos == std::string::npos)
    end_pos = content_type.find('\n', boundary_pos);
  if (end_pos == std::string::npos)
    end_pos = content_type.length();

  return content_type.substr(boundary_pos, end_pos - boundary_pos);
}

std::size_t ServerResponse::parse_multipart_part(const std::string &body,
                                                 const std::string &boundary,
                                                 const std::size_t start_pos,
                                                 std::string &out_filename,
                                                 std::string &out_fieldname,
                                                 std::string &out_data) {
  out_filename = "";
  out_fieldname = "";
  out_data = "";

  // Find the start of this part (after boundary marker)
  const std::string boundary_marker = "--" + boundary;
  size_t part_start = body.find(boundary_marker, start_pos);

  if (part_start == std::string::npos)
    return std::string::npos;

  // Skip past boundary and line ending
  part_start += boundary_marker.length();
  if (part_start < body.length() && body[part_start] == '\r')
    part_start++;
  if (part_start < body.length() && body[part_start] == '\n')
    part_start++;

  // Find end of headers (blank line: \r\n\r\n or \n\n)
  size_t header_end = body.find("\r\n\r\n", part_start);
  size_t skip_length = 4; // \r\n\r\n
  if (header_end == std::string::npos) {
    header_end = body.find("\n\n", part_start);
    skip_length = 2; // \n\n
  }

  if (header_end == std::string::npos)
    return std::string::npos;

  // Extract and parse headers
  std::string headers_section =
      body.substr(part_start, header_end - part_start);

  // Parse Content-Disposition header to extract name and filename
  const size_t disp_pos = headers_section.find("Content-Disposition:");
  if (disp_pos != std::string::npos) {
    size_t line_end = headers_section.find('\n', disp_pos);
    if (line_end == std::string::npos)
      line_end = headers_section.length();

    std::string disp_line =
        headers_section.substr(disp_pos, line_end - disp_pos);

    // Extract name="fieldname"
    size_t name_pos = disp_line.find("name=\"");
    if (name_pos != std::string::npos) {
      name_pos += 6; // Length of 'name="'
      const size_t name_end = disp_line.find('"', name_pos);
      if (name_end != std::string::npos) {
        out_fieldname = disp_line.substr(name_pos, name_end - name_pos);
      }
    }

    // Extract filename="filename.txt"
    size_t filename_pos = disp_line.find("filename=\"");
    if (filename_pos != std::string::npos) {
      filename_pos += 10; // Length of 'filename="'
      const size_t filename_end = disp_line.find('"', filename_pos);
      if (filename_end != std::string::npos) {
        out_filename =
            disp_line.substr(filename_pos, filename_end - filename_pos);
      }
    }
  }

  // Find start of part body (skip blank line)
  const size_t body_start = header_end + skip_length;

  // Find end of part body (next boundary)
  size_t next_boundary = body.find("\r\n--" + boundary, body_start);
  if (next_boundary == std::string::npos)
    next_boundary = body.find("\n--" + boundary, body_start);

  if (next_boundary == std::string::npos)
    next_boundary = body.find("--" + boundary, body_start);

  if (next_boundary == std::string::npos) {
    // Last part - take rest of body
    out_data = body.substr(body_start);
    // Remove trailing CRLF if present
    if (out_data.length() >= 2 &&
        out_data.substr(out_data.length() - 2) == "\r\n")
      out_data = out_data.substr(0, out_data.length() - 2);
    else if (!out_data.empty() && out_data[out_data.length() - 1] == '\n')
      out_data = out_data.substr(0, out_data.length() - 1);
    return std::string::npos;
  }

  // Extract data and trim trailing line ending
  out_data = body.substr(body_start, next_boundary - body_start);
  if (out_data.length() >= 2 &&
      out_data.substr(out_data.length() - 2) == "\r\n")
    out_data = out_data.substr(0, out_data.length() - 2);
  else if (!out_data.empty() && out_data[out_data.length() - 1] == '\n')
    out_data = out_data.substr(0, out_data.length() - 1);

  return next_boundary;
}

Response ServerResponse::delete_method(const Target &target, Response response,
                                       const ServerConfig *config,
                                       const RouteRule *rule, char **envp) {
  if (unlink(target.path.c_str()) == 0) {
    response.status_code = Response::NO_CONTENT;
    return response;
  } else {
    return error_response(config, rule, Response::FORBIDDEN, envp);
  }
}

Response ServerResponse::post_method(const Target &target, Response response,
                                     const ClientSession *client,
                                     const RouteRule *rule,
                                     const Request *request, Session *session,
                                     char **envp) {
  std::cout << utils::debug << "target path: " << target.path << std::endl;
  std::cout << utils::debug << "POST request path: " << request->get_path()
            << std::endl;
  std::cout << utils::debug << "rewritten path: "
            << client->config->get_rewritten_path(request->get_method(),
                                                  request->get_path())
            << std::endl;
  const ServerConfig *config = client->config;

  // Handle login/authentication
  if (request->get_path() == rule->path.to_string() &&
      rule->op == LOGIN_USING) {
    const std::string &body = request->get_body();

    std::string auth_target =
        utils::get_env("PWD", envp) + rule->root.to_string();
    std::cout << "\n"
              << utils::debug << "auth info: " << auth_target << "\n"
              << std::endl;
    std::ifstream file(auth_target.c_str());
    if (file.is_open()) {
      std::string pw;
      std::string id;
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
        std::cout << utils::info << "Authentication SUCCESS for: " << id
                  << std::endl;

        // 1. 브라우저에게 "이 주소로 가라"고 알리는 상태 코드 설정
        // 일반적으로 다른 페이지 이동 시 302 혹은 303을 사용
        response.status_code = Response::FOUND;
        response.redir = "/";
        response.content_type = "text/html";
        response.body = "<html><body>Redirecting...</body></html>";
        response.cookie =
            "session_id=" + session->create_session(id, client->ip) +
            "; Path=/; HttpOnly";
        return response;
      } else {
        std::cout << utils::info << "Authentication FAILED for: " << id
                  << std::endl;

        response.status_code = Response::OK;
        response.content_type = "text/html";
        // 로그인 실패 시 브라우저 자체 Alert 팝업을 띄우고 이전(로그인)
        // 화면으로 다시 돌려보냅니다.
        response.body = "<script>"
                        "alert('invalid id or password');"
                        "window.location.href='/login.html';"
                        "</script>";
        return response;
      }
    } else
      return error_response(config, rule, Response::NOT_FOUND, envp);
  }

  // Handle file uploads
  std::cout << utils::info << "matched rule path: '" << rule->path
            << "' upload_dir = '" << rule->root.to_string() << "'" << std::endl;
  if (rule->root.to_string().empty())
    const_cast<RouteRule *>(rule)->root.to_string() = rule->root.to_string();
  if (!rule->root.to_string().empty()) {
    const std::string &body = request->get_body();
    const std::map<std::string, std::string> &headers = request->get_headers();

    // Check Content-Type for multipart/form-data
    std::map<std::string, std::string>::const_iterator content_type_it =
        headers.find("Content-Type");
    if (content_type_it == headers.end())
      return error_response(config, rule, Response::BAD_REQUEST, envp);

    std::string content_type = content_type_it->second;
    if (content_type.find("multipart/form-data") == std::string::npos)
      return error_response(config, rule, Response::BAD_REQUEST, envp);

    // Check body size limit
    const unsigned int max_body_KB = rule->max_body_KB;
    if (body.length() > static_cast<size_t>(max_body_KB) * 1024) {
      std::cout << utils::warning << "Upload rejected: body size "
                << body.length() << " exceeds limit " << (max_body_KB * 1024)
                << std::endl;
      return error_response(config, rule, Response::PAYLOAD_TOO_LARGE, envp);
    }

    // Extract boundary
    std::string boundary = extract_boundary(content_type);
    if (boundary.empty())
      return error_response(config, rule, Response::BAD_REQUEST, envp);

    std::cout << utils::info << "Boundary: " << boundary << std::endl;

    // Create upload directory if it doesn't exist
    std::string upload_path =
        utils::get_env("PWD", envp) + "/" + rule->root.to_string();
    struct stat st = {};
    if (stat(upload_path.c_str(), &st) != 0) {
      std::cerr << utils::warning << "upload dir missing: " << upload_path
                << " - operator must pre-create" << std::endl;
      return error_response(config, rule, Response::INTERNAL_SERVER_ERR, envp);
    }
    if (!S_ISDIR(st.st_mode)) {
      std::cerr << utils::warning << "upload path exists yet not a directory"
                << std::endl;
      return error_response(config, rule, Response::INTERNAL_SERVER_ERR, envp);
    }
    if (access(upload_path.c_str(), W_OK) != 0) {
      std::cerr << utils::warning << "upload dir not writable" << std::endl;
      return error_response(config, rule, Response::INTERNAL_SERVER_ERR, envp);
    }

    // Parse multipart parts and save files
    std::size_t pos = 0;
    int files_uploaded = 0;
    std::string error_msg;

    while (true) {
      std::string filename, fieldname, part_data;
      std::size_t next_pos = parse_multipart_part(body, boundary, pos, filename,
                                                  fieldname, part_data);

      if (filename.empty() && fieldname.empty())
        break; // No more parts

      if (!filename.empty()) {
        // Validate filename - reject path traversal attempts
        if (filename.find("..") != std::string::npos ||
            filename.find('/') != std::string::npos ||
            filename.find('\\') != std::string::npos) {
          std::cout << utils::info
                    << "Rejected filename with path traversal: " << filename
                    << std::endl;
          error_msg = "Invalid filename";
          break;
        }

        std::string file_path = upload_path + "/";
        file_path += filename;
        std::cout << utils::info << "Uploading file: " << file_path
                  << " (size: " << part_data.length() << ")" << std::endl;
        std::cout << utils::info << "First 20 bytes (hex): ";
        for (size_t i = 0;
             i < std::min(static_cast<size_t>(20), part_data.length()); i++) {
          printf("%02x ", static_cast<unsigned char>(part_data[i]));
        }
        std::cout << std::endl;

        // Try to open file for writing
        std::ofstream outfile(file_path.c_str(), std::ios::binary);
        if (!outfile.is_open()) {
          std::cout << utils::warning
                    << "Failed to open file for writing: " << file_path
                    << std::endl;
          return error_response(config, rule, Response::FORBIDDEN, envp);
        }

        // Write file data
        outfile.write(part_data.c_str(),
                      static_cast<std::streamsize>(part_data.length()));
        if (outfile.fail()) {
          std::cout << utils::warning << "Failed to write file: " << file_path
                    << std::endl;
          outfile.close();
          return error_response(config, rule, Response::FORBIDDEN, envp);
        }

        outfile.close();
        files_uploaded++;
      }

      if (next_pos == std::string::npos)
        break; // No more parts

      pos = next_pos;
    }

    if (!error_msg.empty())
      return error_response(config, rule, Response::BAD_REQUEST, envp);

    if (files_uploaded > 0) {
      response.status_code = Response::OK;
      response.content_type = "text/plain";
      std::ostringstream oss;
      oss << "Successfully uploaded " << files_uploaded << " file(s)";
      response.body = oss.str();
      return response;
    } else {
      response.status_code = Response::BAD_REQUEST;
      response.content_type = "text/plain";
      response.body = "No files uploaded";
      return response;
    }
  }

  return error_response(config, rule, Response::FORBIDDEN, envp);
}

Response ServerResponse::get_method(Target target, Response response,
                                    const ServerConfig *config,
                                    const RouteRule *rule,
                                    const Request *request, char **envp) {
  std::cout << utils::debug << "target path: " << target.path << std::endl;
  // Handle redirects FIRST, before checking if target exists on filesystem
  if (rule->op == REDIRECT) {
    target.type = Response::MOVED_PERMANENTLY;
    response.redir =
        config->get_rewritten_path(request->get_method(), request->get_path());
    response.status_code = Response::MOVED_PERMANENTLY;
    response.content_type = "text/html";
    response.body = "<html><body><h1>301 Moved Permanently</h1></body></html>";
    return response;
  }

  // Then handle error responses (NOT_FOUND_ERR, FORBIDDEN_ERR)
  if (target.type == Response::NOT_FOUND)
    return error_response(config, rule, Response::NOT_FOUND, envp);
  if (target.type == Response::FORBIDDEN)
    return error_response(config, rule, Response::FORBIDDEN, envp);

  if (target.type == IS_DIR && rule->op == AUTOINDEX) {
    DIR *dir = opendir(target.path.c_str());
    if (dir == NULL) {
      if (errno == EACCES) // access denied
        return error_response(config, rule, Response::FORBIDDEN, envp);
      else if (errno == ENOENT) // no such directory
        return error_response(config, rule, Response::NOT_FOUND, envp);
    }
    target.type = Response::OK;
    response.content_type = "text/html";
    response.body = make_autoindex_page(target.path, request->get_path(), dir);
    response.status_code = Response::OK;
  } else {
    if (check_path_type(target.path) != IS_FILE)
      return error_response(config, rule, Response::NOT_FOUND, envp);
    std::ifstream file(target.path.c_str());
    if (file.is_open()) {
      std::ostringstream ss;
      ss << file.rdbuf();
      target.type = Response::OK;
      response.body = ss.str();
      response.status_code = Response::OK;
      file.close();
      // Add ETag and Last-Modified headers
      response.headers["ETag"] = compute_etag(target.path);
      response.headers["Last-Modified"] = get_last_modified(target.path);
    } else {
      return error_response(config, rule, Response::NOT_FOUND, envp);
    }
  }
  return response;
}

Result<Response>
Response::from_cgi_outbuff(std::string const &cgi_out,
                           std::map<std::string, std::string> const &headers) {
  std::cerr << "[CGI-PARSE] raw output (" << cgi_out.length() << " bytes):\n"
            << cgi_out << "\n[CGI-PARSE-END]\n";
  size_t bound_pos = cgi_out.find("\r\n\r\n");
  size_t bound_len = 4;

  if (bound_pos == std::string::npos) {
    bound_pos = cgi_out.find("\n\n");
    bound_len = 2;
  }
  if (bound_pos == std::string::npos)
    return ERR(Response, Errors::bad_gateway);

  std::string header_part(cgi_out.substr(0, bound_pos));
  Response resp;

  resp.headers = headers;
  resp.body = cgi_out.substr(bound_pos + bound_len);

  std::istringstream iss(header_part);
  std::string header_line;
  while (std::getline(iss, header_line) && !iss.fail()) {
    size_t colon_pos = header_line.find(':');
    if (colon_pos == std::string::npos)
      continue;
    std::string header_name = header_line.substr(0, colon_pos);
    std::string header_value = header_line.substr(colon_pos + 1);
    if (!header_value.empty() &&
        header_value[header_value.length() - 1] == '\r')
      header_value = header_value.substr(0, header_value.length() - 1);
    size_t whitespace_pos = header_value.find_first_not_of(" \t");
    if (whitespace_pos == std::string::npos)
      return ERR(Response, Errors::bad_gateway);
    header_value = header_value.substr(whitespace_pos);
    whitespace_pos = header_value.find_last_not_of(" \t");
    if (whitespace_pos != std::string::npos)
      header_value = header_value.substr(0, whitespace_pos + 1);
    if (!utils::is_header_name(header_name) ||
        !utils::is_header_value(header_value))
      return ERR(Response, Errors::bad_gateway);
    std::string header_name_lower(header_name.size(), '\0');
    std::transform(header_name.begin(), header_name.end(),
                   header_name_lower.begin(), utils::tolower);
    for (std::map<std::string, std::string>::iterator it = resp.headers.begin();
         it != resp.headers.end(); ++it) {
      std::string header_name_lower_(it->first.size(), '\0');
      std::transform(it->first.begin(), it->first.end(),
                     header_name_lower_.begin(), utils::tolower);
      if (header_name_lower_ == header_name_lower) {
        std::cerr << "[CGI-PARSE-DUP] incoming='" << header_name
                  << "' collides with seeded='" << it->first << "'"
                  << std::endl;
        return ERR(Response, Errors::bad_gateway);
      }
    }
    resp.headers[header_name] = header_value;
  }

  bool status_found = false, content_length_found = false,
       content_type_found = false;
  for (std::map<std::string, std::string>::iterator it = resp.headers.begin();
       it != resp.headers.end();) {
    std::string header_name_lower(it->first.size(), '\0');
    std::transform(it->first.begin(), it->first.end(),
                   header_name_lower.begin(), utils::tolower);

    if (header_name_lower == "content-length") {
      std::istringstream iss_(it->second);
      iss_ >> resp.content_length;
      if (iss_.fail())
        return ERR(Response, Errors::bad_gateway);
      resp.headers.erase(it++);
      content_length_found = true;
      continue;
    }

    if (header_name_lower == "status") {
      std::istringstream iss_(it->second);
      unsigned short status_code;
      iss_ >> status_code;
      if (iss_.fail() || status_code < 100 || status_code > 599)
        return ERR(Response, Errors::bad_gateway);
      resp.status_code = DefaultError::int_to_status_code(status_code);
      resp.headers.erase(it++);
      status_found = true;
      continue;
    }

    if (header_name_lower == "content-type") {
      resp.content_type = it->second;
      resp.headers.erase(it++);
      content_type_found = true;
      continue;
    }

    if (header_name_lower == "set-cookie") {
      resp.cookie = it->second;
      resp.headers.erase(it++);
      continue;
    }

    if (header_name_lower == "connection") {
      resp.keep_alive = it->second == "keep-alive";
      resp.headers.erase(it++);
      continue;
    }

    ++it;
  }

  if (!status_found)
    resp.status_code = Response::OK;
  if (!content_length_found)
    resp.content_length = resp.body.length();
  if (!content_type_found)
    resp.content_type = "text/html";
  if (resp.status_code == Response::MOVED_PERMANENTLY ||
      resp.status_code == Response::FOUND) {
    if (resp.headers.find("location") != resp.headers.end())
      resp.redir = get_string_from_map(resp.headers, "location");
    else if (resp.headers.find("Location") != resp.headers.end())
      resp.redir = get_string_from_map(resp.headers, "Location");
    else
      return ERR(Response, Errors::bad_gateway);
    resp.headers.erase("location");
    resp.headers.erase("Location");
  }

  return OK(Response, resp);
}
