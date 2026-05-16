#ifndef PATHPATTERN_HPP
#define PATHPATTERN_HPP

#include "../utils.hpp"

class PathPattern {
private:
  std::vector<std::string> path;

  std::string extract_relative_path(const std::string &pattern,
                                    const std::string &target) const;
  bool wildcard_match(const std::string &pattern,
                      const std::string &target) const;
  std::string apply_wildcards(const std::string &to_pattern,
                              const std::vector<std::string> &wildcards) const;
  bool extract_wildcards(const std::string &pattern, const std::string &target,
                         std::vector<std::string> &wildcards) const;

public:
  PathPattern() : path() {}
  PathPattern(const std::string &pathStr) {
    if (pathStr == "/" || pathStr.empty()) {
      path.push_back("/");
      return;
    }
    path = utils::string_split(pathStr, "/");
    if (!pathStr.empty() && pathStr[pathStr.length() - 1] == '/')
      path.push_back("/");
    if (pathStr[0] == '/')
      path[0] = "/" + path[0];
  }
  PathPattern(const PathPattern &other) : path(other.path) {}

  void change_path(std::size_t i, std::string data) { path[i] = data; }
  bool is_wildcard() const { return (path.size() == 1 && path[0] == "*"); }
  bool matches(const PathPattern &other) const;
  bool matches(const std::string &pathStr) const;
  const std::vector<std::string> &get_path(void) const { return path; }
  std::string to_string() const;

  std::string rewrite_path(const PathPattern &request_path,
                           const PathPattern &to_pattern) const;
};

#endif