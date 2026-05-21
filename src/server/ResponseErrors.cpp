#include "ResponseErrors.hpp"
#include "../core/utils.hpp"
#include "DefaultError.hpp"
#include "ResponseUtils.hpp"
#include <fstream>
#include <sstream>

Response ResponseErrors::error_response(const ServerConfig *config,
                                        const RouteRule *rule,
                                        const Response::StatusCode error_code,
                                        char **envp) {
  std::string err_page = utils::get_env("PWD", envp) +
                         get_string_from_map(rule->error_pages, error_code);
  if (err_page.empty())
    return DefaultError::default_err_response(error_code);

  Result<int> path_type_res = ResponseUtils::check_path_type(err_page);
  if (!path_type_res.has_value() || path_type_res.value() != IS_FILE)
    return DefaultError::default_err_response(error_code);

  std::ifstream file(err_page.c_str());
  if (file.is_open()) {
    Response response;
    response.status_code = error_code;
    response.headers = config->get_header();
    std::string ext = ResponseUtils::find_file_type(err_page);
    response.content_type = ResponseUtils::get_mime_type_for_extension(ext);
    std::ostringstream ss;
    ss << file.rdbuf();
    response.body = ss.str();
    response.content_length = response.body.length();
    file.close();
    return response;
  }
  return DefaultError::default_err_response(error_code);
}

Target ResponseErrors::resolve_target(const RouteRule *rule,
                                      const ServerConfig *config,
                                      const Request *request, char **envp) {
  Target target;
  if (rule == NULL) {
    target.type = Response::NOT_FOUND;
    return target;
  }

  Result<std::string> rewritten =
      config->get_rewritten_path(request->get_method(), request->get_path());
  if (!rewritten.has_value()) {
    target.type = Response::NOT_FOUND;
    return target;
  }
  const std::string root = rewritten.value();

  target.path = utils::get_env("PWD", envp);
  Result<int> type_res = ResponseUtils::check_path_type(target.path + root);

  if (type_res.has_value()) {
    const int type = type_res.value();
    if (type == IS_DIR) {
      if (rule->op == SERVE_FROM && request->get_path() == "/") {
        target.path += rule->index;
      } else {
        target.path += root;
      }
      Result<int> check_res = ResponseUtils::check_path_type(target.path);
      target.type =
          check_res.has_value() ? check_res.value() : Response::NOT_FOUND;
    } else {
      target.path += root;
      Result<int> check_res = ResponseUtils::check_path_type(target.path);
      target.type =
          check_res.has_value() ? check_res.value() : Response::NOT_FOUND;
    }
  } else {
    if (type_res.error() == Errors::not_found) {
      target.path +=
          get_string_from_map(rule->error_pages, Response::NOT_FOUND);
      target.type = Response::NOT_FOUND;
    } else if (type_res.error() == Errors::access_denied) {
      target.path +=
          get_string_from_map(rule->error_pages, Response::FORBIDDEN);
      target.type = Response::FORBIDDEN;
    } else {
      target.path += root;
      target.type = Response::NOT_FOUND;
    }
  }

  return target;
}
