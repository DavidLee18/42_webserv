#include "Response.hpp"
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

  std::cout << "path: " << path << std::endl;
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
    target.path = "/spool/www/error/404.html";
    return target;
  }
  std::cout << "resolve_target request path: " << request->get_path() << std::endl;
  std::string root =
      config->get_rewritten_path(request->get_method(), request->get_path());

  target.path = get_pwd();
  std::cout << "Root: " << root << std::endl;
  int type = check_path_type(target.path + root);
  if (type == IS_DIR) {
      target.path += root + "/";
      if (rule->op != AUTOINDEX)
       target.path += rule->index;
  } else if (type == NOT_FOUND_ERR)
    target.path += config->get_rewritten_path(
        request->get_method(),
        get_string_from_map(rule->error_pages, NOT_FOUND_ERR));
  else if (type == FORBIDDEN_ERR)
    target.path += config->get_rewritten_path(
        request->get_method(),
        get_string_from_map(rule->error_pages, FORBIDDEN_ERR));
  else
    target.path += root;
  target.type = check_path_type(target.path);

  std::cout << "target path: " << target.path << std::endl;
  std::cout << "rule index: " << rule->index << std::endl;
  return target;
}


Response ServerResponse::error_response(const ServerConfig *config,
                                        const RouteRule *rule,
                                        int err_code)
{
  Response response;
  std::string err_page = get_string_from_map(rule->error_pages, err_code);

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


Response
ServerResponse::http_response(const Request *request, const ServerConfig *config,
                   const std::map<std::string, std::string> mime_type) {
  const RouteRule *rule =
      config->find_route(request->get_method(), request->get_path());
  Response response;
  Target target = resolve_target(rule, config, request);

  std::cout << std::endl;
  if (rule == NULL) {
    std::cout << "rule: NULL" << std::endl;
  } else {
    std::cout << "rule path   : [" << rule->path.to_string() << "]" << std::endl;
    std::cout << "rule root   : [" << rule->root.to_string() << "]" << std::endl;
  }

  std::cout << std::endl;
  if (rule != NULL) {
    PathPattern request_path(request->get_path());

    std::cout << "request path: [" << request_path.to_string() << "]" << std::endl;

    std::cout << std::endl;
    std::string new_path = rule->path.rewrite_path(request->get_path(), rule->root);
    std::cout << "new path    : [" << new_path << "]" << std::endl;
  }

std::cout << "===============================\n" << std::endl;

  std::cout << "====checking===" << target.type << rule->op << std::endl;
  response.mime_type =
      get_string_from_map(mime_type, find_file_type(target.path));
  if (rule->op == REDIRECT) {
    std::cout << "=== redirection ===" << std::endl;
    target.type = MOVED_PERMANENTLY;
    response.redir = config->get_rewritten_path(request->get_method(), request->get_path());
    response.status_code = status_code_to_string(target.type);
    response.mime_type = "text/html";
    response.body = "<html><body><h1>301 Moved Permanently</h1></body></html>";
  } else if (target.type == IS_DIR && rule->op == AUTOINDEX) {
    DIR *dir = opendir(target.path.c_str());
    if (dir == NULL) {
      return error_response(config, rule, FORBIDDEN_ERR); // TODO: 폴더를 열 권한이 없거나 없으면 빈 문자열 반환 (나중에 403처리)
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
  // 1. HTML 기본 뼈대 작성
  std::string html =
      "<html><head><title>Index of " + req_uri + "</title></head><body>\r\n";
  html += "<h1>Index of " + req_uri + "</h1><hr><pre>\r\n";

  struct dirent *entity;

  // 2. 디렉토리 안의 파일들을 하나씩 읽기
  while ((entity = readdir(dir)) != NULL) {
    std::string name = entity->d_name;

    // 현재 폴더(.)는 굳이 보여줄 필요가 없으니 스킵
    if (name == ".")
      continue;

    // 절대 경로를 합쳐서 진짜 폴더인지 검사
    std::string full_item_path = real_path + "/" + name;
    int type = check_path_type(full_item_path);
    if (type == IS_DIR)
      name += "/";
    html += "<a href=\"" + name + "\">" + name + "</a>\r\n<br>\r\n";
  }
  html += "</pre><hr></body></html>";
  closedir(dir);

  return html;
}
