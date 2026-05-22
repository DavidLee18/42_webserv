#include "ResponseUtils.hpp"
#include "../core/utils.hpp"
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

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

std::string ResponseUtils::get_http_date() {
  const time_t now = std::time(NULL);
  const struct tm *timeinfo = std::gmtime(&now);
  char buffer[100];
  std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
  return std::string(buffer);
}

std::string ResponseUtils::find_file_type(const std::string &path) {
  std::vector<std::string> file_type = utils::string_split(path, ".");

  if (file_type.size() <= 1)
    return "default";
  return file_type.back();
}

std::string ResponseUtils::get_mime_type_for_extension(const std::string &ext) {
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
  else if (ext == "webp")
    return "image/webp";
  return "text/html";
}

std::string ResponseUtils::get_mime_type_for_extension(
    const std::string &ext,
    const std::map<std::string, std::string> &mime_map) {
  // First, check the config mime_map (from types block in config)
  std::map<std::string, std::string>::const_iterator it = mime_map.find(ext);
  if (it != mime_map.end())
    return it->second;

  // Fallback to hardcoded mappings if not in config
  return get_mime_type_for_extension(ext);
}

Result<int> ResponseUtils::check_path_type(const std::string &path) {
  struct stat info = {};

  if (stat(path.c_str(), &info) != 0) {
    return ERR(int, Errors::not_found);
  }
  if (access(path.c_str(), R_OK) != 0) {
    return ERR(int, Errors::access_denied);
  }
  if (S_ISDIR(info.st_mode)) {
    return OK(int, IS_DIR);
  }
  if (S_ISREG(info.st_mode)) {
    if (path.length() > 4) {
      if (path.rfind(".cgi") == path.length() - 4) {
        return ERR(int, Errors::forbidden);
      }
    }
    return OK(int, IS_FILE);
  }
  return ERR(int, Errors::bad_request);
}

Result<std::string> ResponseUtils::compute_etag(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) == -1) {
    return ERR(std::string, "cannot stat file for etag computation");
  }

  std::ostringstream oss;
  oss << "\"" << st.st_ino << "-" << st.st_size << "-" << st.st_mtime << "\"";
  return OK(std::string, oss.str());
}

Result<std::string> ResponseUtils::get_last_modified(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) == -1)
    return ERR(std::string, "cannot stat file for last-modified header");

  char buf[100];
  struct tm *tm_info = gmtime(&st.st_mtime);
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
  return OK(std::string, std::string(buf));
}

std::string ResponseUtils::escape_html(const std::string &input) {
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
