#include "ResponseHandlers.hpp"
#include "../cgi_1_1/CgiDelegate.hpp"
#include "../core/utils.hpp"
#include "AutoindexResponse.hpp"
#include "DefaultError.hpp"
#include "MultipartParser.hpp"
#include "ResponseErrors.hpp"
#include "ResponseUtils.hpp"
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

Result<Void> ResponseHandlers::register_cgi(
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

Response ResponseHandlers::delete_method_response(const Target &target,
                                                  Response response,
                                                  const ServerConfig *config,
                                                  const RouteRule *rule,
                                                  char **envp) {
  std::cout << utils::debug << "target path: " << target.path << std::endl;
  if (unlink(target.path.c_str()) == 0) {
    response.status_code = Response::NO_CONTENT;
    return response;
  }

  switch (errno) {
  case ENOENT:
    return ResponseErrors::error_response(config, rule, Response::NOT_FOUND,
                                          envp);
  case EISDIR:
    return ResponseErrors::error_response(config, rule, Response::CONFLICT,
                                          envp);
  case EACCES:
  case EPERM:
    return ResponseErrors::error_response(config, rule, Response::FORBIDDEN,
                                          envp);
  default:
    return ResponseErrors::error_response(config, rule, Response::FORBIDDEN,
                                          envp);
  }
}

Response ResponseHandlers::post_method_response(const Target &target,
                                                Response response,
                                                const ClientSession *client,
                                                const RouteRule *rule,
                                                const Request *request,
                                                Session *session, char **envp) {
  const ServerConfig *config = client->config;

  std::cout << utils::debug << "target path: " << target.path << std::endl;
  if (request->get_path() == rule->path.to_string() &&
      rule->op == LOGIN_USING) {
    const std::string &body = request->get_body();

    std::string auth_target =
        utils::get_env("PWD", envp) + rule->root.to_string();
    std::ifstream file(auth_target.c_str());
    if (file.is_open()) {
      std::string pw;
      std::string id;
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

        Result<std::string> session_result =
            session->create_session(id, client->ip);
        if (!session_result.has_value()) {
          return ResponseErrors::error_response(
              config, rule, Response::INTERNAL_SERVER_ERR, envp);
        }

        response.status_code = Response::FOUND;
        response.redir = "/";
        response.content_type = "text/html";
        response.body = "<html><body>Redirecting...</body></html>";
        response.cookie =
            "session_id=" + session_result.value() + "; Path=/; HttpOnly";
        return response;
      } else {
        std::cout << utils::info << "Authentication FAILED for: " << id
                  << std::endl;

        response.status_code = Response::OK;
        response.content_type = "text/html";
        response.body = "<script>"
                        "alert('invalid id or password');"
                        "window.location.href='/login.html';"
                        "</script>";
        return response;
      }
    } else
      return ResponseErrors::error_response(config, rule, Response::NOT_FOUND,
                                            envp);
  }

  std::cout << utils::info << "matched rule path: '" << rule->path
            << "' upload_dir = '" << rule->root.to_string() << "'" << std::endl;
  if (!rule->root.to_string().empty()) {
    const std::string &body = request->get_body();
    const std::map<std::string, std::string> &headers = request->get_headers();

    std::map<std::string, std::string>::const_iterator content_type_it =
        headers.find("Content-Type");
    if (content_type_it == headers.end())
      return ResponseErrors::error_response(config, rule, Response::BAD_REQUEST,
                                            envp);

    std::string content_type = content_type_it->second;
    if (content_type.find("multipart/form-data") == std::string::npos)
      return ResponseErrors::error_response(config, rule, Response::BAD_REQUEST,
                                            envp);

    const unsigned int max_body_KB = rule->max_body_KB;
    if (body.length() > static_cast<size_t>(max_body_KB) * 1024) {
      std::cout << utils::warning << "Upload rejected: body size "
                << body.length() << " exceeds limit " << (max_body_KB * 1024)
                << std::endl;
      return ResponseErrors::error_response(config, rule,
                                            Response::PAYLOAD_TOO_LARGE, envp);
    }

    std::string boundary = MultipartParser::extract_boundary(content_type);
    if (boundary.empty())
      return ResponseErrors::error_response(config, rule, Response::BAD_REQUEST,
                                            envp);

    std::cout << utils::info << "Boundary: " << boundary << std::endl;

    std::string upload_path =
        utils::get_env("PWD", envp) + "/" + rule->root.to_string();
    struct stat st = {};
    if (stat(upload_path.c_str(), &st) != 0) {
      std::cerr << utils::warning << "upload dir missing: " << upload_path
                << " - operator must pre-create" << std::endl;
      if (errno == EACCES) {
        return ResponseErrors::error_response(config, rule, Response::FORBIDDEN,
                                              envp);
      }
      return ResponseErrors::error_response(
          config, rule, Response::INTERNAL_SERVER_ERR, envp);
    }
    if (!S_ISDIR(st.st_mode)) {
      std::cerr << utils::warning << "upload path exists yet not a directory"
                << std::endl;
      return ResponseErrors::error_response(
          config, rule, Response::INTERNAL_SERVER_ERR, envp);
    }
    if (access(upload_path.c_str(), W_OK) != 0) {
      std::cerr << utils::warning << "upload dir not writable" << std::endl;
      if (errno == EACCES) {
        return ResponseErrors::error_response(config, rule, Response::FORBIDDEN,
                                              envp);
      }
      return ResponseErrors::error_response(
          config, rule, Response::INTERNAL_SERVER_ERR, envp);
    }

    std::size_t pos = 0;
    int files_uploaded = 0;
    std::string error_msg;

    while (true) {
      std::string filename, fieldname, part_data;
      std::size_t next_pos = MultipartParser::parse_multipart_part(
          body, boundary, pos, filename, fieldname, part_data);

      if (filename.empty() && fieldname.empty())
        break;

      if (!filename.empty()) {
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
          std::cout << std::hex << std::uppercase << std::setw(2)
                    << std::setfill('0')
                    << static_cast<unsigned int>(
                           static_cast<unsigned char>(part_data[i]))
                    << ' ';
        }
        std::cout << std::endl;

        std::ofstream outfile(file_path.c_str(), std::ios::binary);
        if (!outfile.is_open()) {
          std::cout << utils::warning
                    << "Failed to open file for writing: " << file_path
                    << std::endl;
          if (errno == EACCES || errno == EPERM) {
            return ResponseErrors::error_response(config, rule,
                                                  Response::FORBIDDEN, envp);
          }
          return ResponseErrors::error_response(
              config, rule, Response::INTERNAL_SERVER_ERR, envp);
        }

        outfile.write(part_data.c_str(),
                      static_cast<std::streamsize>(part_data.length()));
        if (outfile.fail()) {
          std::cout << utils::warning << "Failed to write file: " << file_path
                    << std::endl;
          outfile.close();
          return ResponseErrors::error_response(
              config, rule, Response::INTERNAL_SERVER_ERR, envp);
        }

        outfile.close();
        files_uploaded++;
      }

      if (next_pos == std::string::npos)
        break;

      pos = next_pos;
    }

    if (!error_msg.empty())
      return ResponseErrors::error_response(config, rule, Response::BAD_REQUEST,
                                            envp);

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

  return ResponseErrors::error_response(config, rule, Response::FORBIDDEN,
                                        envp);
}

Response ResponseHandlers::get_method_response(Target target, Response response,
                                               const ServerConfig *config,
                                               const RouteRule *rule,
                                               const Request *request,
                                               char **envp) {
  std::cout << utils::debug << "target path: " << target.path << std::endl;
  if (rule->op == REDIRECT) {
    target.type = Response::MOVED_PERMANENTLY;
    Result<std::string> rewritten =
        config->get_rewritten_path(request->get_method(), request->get_path());
    if (rewritten.has_value()) {
      response.redir = rewritten.value();
    } else {
      response.redir = "/";
    }
    response.status_code = Response::MOVED_PERMANENTLY;
    response.content_type = "text/html";
    response.body = "<html><body><h1>301 Moved Permanently</h1></body></html>";
    return response;
  }

  if (target.type == Response::NOT_FOUND)
    return ResponseErrors::error_response(config, rule, Response::NOT_FOUND,
                                          envp);
  if (target.type == Response::FORBIDDEN)
    return ResponseErrors::error_response(config, rule, Response::FORBIDDEN,
                                          envp);

  if (target.type == IS_DIR && rule->op == AUTOINDEX) {
    DIR *dir = opendir(target.path.c_str());
    if (dir == NULL) {
      if (errno == EACCES) {
        return ResponseErrors::error_response(config, rule, Response::FORBIDDEN,
                                              envp);
      } else if (errno == ENOENT) {
        return ResponseErrors::error_response(config, rule, Response::NOT_FOUND,
                                              envp);
      } else {
        return ResponseErrors::error_response(
            config, rule, Response::INTERNAL_SERVER_ERR, envp);
      }
    }
    target.type = Response::OK;
    response.content_type = "text/html";
    Result<std::string> autoindex_result =
        AutoindexResponse::generate(target.path, request->get_path(), dir);
    if (!autoindex_result.has_value()) {
      closedir(dir);
      return ResponseErrors::error_response(
          config, rule, Response::INTERNAL_SERVER_ERR, envp);
    }
    response.body = autoindex_result.value();
    response.status_code = Response::OK;
  } else {
    Result<int> path_type_result = ResponseUtils::check_path_type(target.path);
    if (!path_type_result.has_value() || path_type_result.value() != IS_FILE)
      return ResponseErrors::error_response(config, rule, Response::NOT_FOUND,
                                            envp);

    std::ifstream file(target.path.c_str());
    if (file.is_open()) {
      struct stat st;
      if (stat(target.path.c_str(), &st) != 0) {
        file.close();
        return ResponseErrors::error_response(config, rule, Response::NOT_FOUND,
                                              envp);
      }
      response.content_length = static_cast<size_t>(st.st_size);
      response.file_path = target.path;
      target.type = Response::OK;
      response.status_code = Response::OK;
      file.close();
    } else {
      return ResponseErrors::error_response(config, rule, Response::NOT_FOUND,
                                            envp);
    }
  }
  return response;
}

Response ResponseHandlers::http_response(
    const Request *request, const ClientSession *client,
    const std::map<std::string, std::string> &mime_type, Session *session,
    char **envp) {
  const ServerConfig *config = client->config;
  if (config == NULL)
    return DefaultError::default_err_response(Response::INTERNAL_SERVER_ERR);

  Response response;
  response.keep_alive = request->has_keep_alive();
  Result<RouteRule> rule_res =
      config->find_route(request->get_method(), request->get_path());
  if (!rule_res.has_value()) {
    response = DefaultError::default_err_response(Response::NOT_FOUND);
    response.headers = config->get_header();
    return response;
  }
  RouteRule rule_copy = rule_res.value();
  const RouteRule *rule = &rule_copy;
  if (request->get_path().find("../") != std::string::npos) {
    std::cerr << utils::error << "request path: " << request->get_path()
              << std::endl;
    response = DefaultError::default_err_response(Response::BAD_REQUEST);
    response.headers = config->get_header();
    return response;
  }

  Result<size_t> res_content_len = request->get_content_length();
  if (res_content_len.has_value()) {
    size_t content_len = res_content_len.value();
    if (content_len > static_cast<size_t>(rule->max_body_KB * 1024))
      return ResponseErrors::error_response(config, rule,
                                            Response::PAYLOAD_TOO_LARGE, envp);
  }

  response.headers = config->get_header();
  const Target target =
      ResponseErrors::resolve_target(rule, config, request, envp);
  const std::string session_id = request->get_cookie_value("session_id");
  const SessionData *user_session = NULL;

  if (!session_id.empty())
    user_session = session->get_session(session_id);
  if (user_session) {
    std::cout << utils::info
              << "[Authentication] Valid user session found! User ID: "
              << user_session->user_id << std::endl;
  } else {
    if (request->get_method() == Request::DELETE) {
      std::cerr << utils::error
                << "[Authentication] Blocked DELETE request. No valid session."
                << std::endl;
      response = ResponseErrors::error_response(config, rule,
                                                Response::UNAUTHORIZED, envp);
      response.headers = config->get_header();
      return response;
    } else {
      std::cout << utils::info
                << "[Authentication] No valid session. Guest user."
                << std::endl;
    }
  }

  if (!user_session && !rule->auth_info.empty()) {
    std::cerr << utils::error
              << "[Authentication] No valid session."
              << std::endl;
    response = ResponseErrors::error_response(config, rule,
                                              Response::UNAUTHORIZED, envp);
    response.headers = config->get_header();
    return response;
  }

  switch (request->get_method()) {
  case Request::DELETE:
    response = delete_method_response(target, response, config, rule, envp);
    break;
  case Request::POST:
    response = post_method_response(target, response, client, rule, request,
                                    session, envp);
    break;
  case Request::GET:
    response =
        get_method_response(target, response, config, rule, request, envp);
    break;
  default:
    std::cerr << utils::error
              << "method not allowed: " << request->get_method_string()
              << std::endl;
    response = ResponseErrors::error_response(
        config, rule, Response::METHOD_NOT_ALLOWED, envp);
    break;
  }

  if (response.file_path.empty())
    response.content_length = response.body.length();

  if (response.content_type.empty()) {
    std::string ext = ResponseUtils::find_file_type(target.path);
    std::string mapped = get_string_from_map(mime_type, ext);
    if (!mapped.empty())
      response.content_type = mapped;
    else
      response.content_type =
          ResponseUtils::get_mime_type_for_extension(ext, mime_type);
  }
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
      return ResponseErrors::error_response(config, rule,
                                            Response::METHOD_NOT_ALLOWED, envp);
    }
  } else {
    return response;
  }
}
