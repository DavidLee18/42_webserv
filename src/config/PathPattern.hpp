#ifndef PATHPATTERN_HPP
#define PATHPATTERN_HPP

#include "../utils.hpp"

/**
 * @class PathPattern
 * @brief 설정 파일의 경로 패턴을 분리하여 저장하고 요청 URL과의 매칭에 사용하는
 * 클래스
 *
 * - 경로를 '/' 단위로 분리하여 각 요소를 비교할 수 있도록 관리한다.
 *
 * - 이때 '*' 문자는 와일드카드로 해석하며, 해당 위치의 임의의 경로 문자열과
 * 일치하는 것으로 처리한다.
 *
 * 예를 들어,
 * 설정 파일에 /download/{wildcard}/mp3/{wildcard} 와 같은 패턴이 정의되어 있을
 * 때, 요청 URL이 /download/asd/mp3/asd 이면 동일한 경로 패턴으로 판단할 수
 * 있다.
 */
class PathPattern {
private:
  /**
   * @var path
   * @brief 설정 파일의 경로 패턴을 분리하여 저장하는 멤버 변수
   */
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

  void add_path(std::string data) {
    path.push_back(data);
    return;
  }
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