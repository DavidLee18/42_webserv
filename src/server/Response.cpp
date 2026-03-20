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

std::string Response::status_code_to_string(int status_code) {
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

std::string Response::get_pwd() {
  char buffer[1024];
  if (getcwd(buffer, sizeof(buffer)) != NULL) {
    return std::string(buffer);
  }
  return "";
}

int Response::check_path_type(const std::string &path) {
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

std::string find_file_type(std::string path) {
  std::vector<std::string> file_type = string_split(path, ".");
  std::cout << "file type: " << file_type.back() << std::endl;

  if (file_type.size() <= 1)
    return "default";
  return file_type.back();
}

HttpResponse
Response::generate(const Request *request, const ServerConfig *config,
                   const std::map<std::string, std::string> mime_type) {
  const RouteRule *rule =
      config->findRoute(request->get_method(), request->get_path());
  HttpResponse response;
  Target target = resolve_target(rule, config, request);

  response.mime_type =
      get_string_from_map(mime_type, find_file_type(target.path));
  if (target.type == IS_DIR && rule->op == AUTOINDEX) {
    target.type = OK;
    response.mime_type = "html";
    response.body = make_autoindex_page(target.path, request->get_path());
    response.status_code = status_code_to_string(target.type);
  } else {
    std::ifstream file(target.path.c_str());
    std::cout << "Target path: " << target.path << std::endl;
    if (file.is_open()) {
      std::ostringstream ss;
      ss << file.rdbuf();
      target.type = OK;
      response.body = ss.str();
      response.status_code = status_code_to_string(target.type);
      file.close();
    }
  }
  return response;
}

Target Response::resolve_target(const RouteRule *rule, const ServerConfig *config, const Request *request) {
  Target target;
  if (rule == NULL) {
    target.type = NOT_FOUND_ERR;
    target.path = get_string_from_map(rule->errorPages, NOT_FOUND_ERR);
    return target;
  }
  std::string root = config->Get_to(request->get_method(), request->get_path());

  target.path = get_pwd();
  std::cout << "Root: " << target.path + root << std::endl;
  int type = check_path_type(target.path + root);
  if (type == IS_DIR && rule->op != AUTOINDEX) {
    if (rule->index.empty())
      target.path += config->Get_to(request->get_method(), "/index.html");
    else
      target.path += config->Get_to(request->get_method(), "/" + rule->index);
  } else if (type == NOT_FOUND_ERR)
    target.path += config->Get_to(request->get_method(), get_string_from_map(rule->errorPages, NOT_FOUND_ERR));
  else if (type == FORBIDDEN_ERR)
    target.path += get_string_from_map(rule->errorPages, FORBIDDEN_ERR);
  else
    target.path += root;
  target.type = type;

  return target;
}

std::string Response::make_autoindex_page(const std::string &real_path,
                                          const std::string &req_uri) {
  DIR *dir = opendir(real_path.c_str());
  if (dir == NULL) {
    return ""; // 폴더를 열 권한이 없거나 없으면 빈 문자열 반환 (나중에 403
               // 처리)
  }

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
