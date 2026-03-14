#include "Response.hpp"
#include "../cgi_1_1.h"
#include "ParsingUtils.hpp"

const std::map<int, std::string> Response::status_code =
    Response::init_status_code();

std::map<int, std::string> Response::init_status_code() {
  std::map<int, std::string> status;
  status[200] = "200 OK";
  status[301] = "301 Moved Permanently";
  status[400] = "400 Bad Request";
  status[403] = "403 Forbidden";
  status[404] = "404 Not Found";
  status[405] = "405 Method Not Allowed";
  status[413] = "413 Payload Too Large";
  status[500] = "500 Internal Server Error";
  return status;
}

std::string Response::get_pwd() {
  char buffer[1024];
  if (getcwd(buffer, sizeof(buffer)) != NULL) {
    return std::string(buffer);
  }
  return "";
}

std::string Response::error_file_path(int error_code) {
  if (error_code == 400)
    return get_pwd() + "/spool/www/error/400.html";
  else if (error_code == 403)
    return get_pwd() + "/spool/www/error/403.html";
  else if (error_code == 404)
    return get_pwd() + "/spool/www/error/404.html";
  return get_pwd() + "/spool/www/error/500.html";
}

int Response::check_path_type(const std::string &path) {
  struct stat info;

  if (stat(path.c_str(), &info) != 0)
    return PATH_ERROR;
  else if (access(path.c_str(), R_OK) != 0)
    return PATH_ERROR;
  else if (S_ISDIR(info.st_mode))
    return IS_DIR;
  else if (S_ISREG(info.st_mode))
    return IS_FILE;
  return PATH_ERROR;
}

std::string find_file_type(std::string path)
{
  std::vector<std::string> file_type = string_split(path, ".");
  std::cout << "file type: " << file_type << std::endl;

  if (file_type.size() <= 1)
    return "default";
  return file_type.back();
}

HttpResponse Response::generate(const Request *request,
                                const ServerConfig *config) {
  HttpResponse response;
  Path path = resolve_path(request, config);

  if (path.type == IS_FILE)
    response.file_type = find_file_type(path.route);
  else
    response.file_type = "default";
  if (path.type == IS_FILE || path.type == IS_DIR)
    path.type = OK;
  std::ifstream file(path.route.c_str());
  if (file.is_open()) {
    std::ostringstream ss;
    ss << file.rdbuf();
    response.body = ss.str();
    response.status_code = status_code.at(path.type);
    file.close();
  }
  return response;
}

Path Response::resolve_path(const Request *request,
                            const ServerConfig *config) {
  const RouteRule *rule = config->findRoute(request->get_method(), request->path());
  Path path;

  if (rule == NULL) {
    path.type = NOT_FOUND_ERR;
    path.route = error_file_path(NOT_FOUND_ERR);
    return path;
  }

  std::string root = get_pwd() + rule->root.toString();
  size_t pos = root.find('*');
  if (pos != std::string::npos && pos + 1 == root.length() && pos > 0)
  {
    root.erase(pos, 1);
    --pos;
    if (root[pos] == '/')
     root.erase(pos, 1);
  }

  if (request->path() == "/")
    path.route = root;
  else
    path.route = root + request->path();
  path.type = check_path_type(path.route);

  return path;
}
