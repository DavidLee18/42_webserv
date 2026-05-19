#include "ResponseErrors.hpp"
#include "ResponseUtils.hpp"
#include "DefaultError.hpp"
#include "../utils/utils.hpp"
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

  Result<int> path_type_result = ResponseUtils::check_path_type(err_page);
  if (!path_type_result.has_value() || path_type_result.value() != IS_FILE)
    return DefaultError::default_err_response(error_code);

  std::ifstream file(err_page.c_str());
  if (file.is_open()) {
    Response response;
    response.status_code = error_code;
    response.headers = config->get_header();
    response.content_type =
        ResponseUtils::get_mime_type_for_extension(ResponseUtils::find_file_type(err_page));
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

Target ResponseErrors::resolve_target(const RouteRule *rule,
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
  Result<int> type_result = ResponseUtils::check_path_type(target.path + root);

  if (type_result.has_value()) {
    const int type = type_result.value();
    if (type == IS_DIR) {
      if (rule->op == SERVE_FROM && request->get_path() == "/") {
        target.path += rule->index;
      } else {
        target.path += root;
      }
      Result<int> check_result = ResponseUtils::check_path_type(target.path);
      target.type = check_result.has_value() ? check_result.value() : Response::NOT_FOUND;
    } else {
      target.path += root;
      Result<int> check_result = ResponseUtils::check_path_type(target.path);
      target.type = check_result.has_value() ? check_result.value() : Response::NOT_FOUND;
    }
  } else {
    if (type_result.error() == Errors::not_found) {
      target.path += get_string_from_map(rule->error_pages, Response::NOT_FOUND);
      target.type = Response::NOT_FOUND;
    } else if (type_result.error() == Errors::access_denied) {
      target.path += get_string_from_map(rule->error_pages, Response::FORBIDDEN);
      target.type = Response::FORBIDDEN;
    } else {
      target.path += root;
      target.type = Response::NOT_FOUND;
    }
  }

  return target;
}
