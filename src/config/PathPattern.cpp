#include "PathPattern.hpp"

bool PathPattern::wildcard_match(const std::string &pattern,
                                 const std::string &target) const {
  std::size_t p = 0;
  std::size_t t = 0;

  std::size_t last_star = std::string::npos;
  std::size_t last_match = std::string::npos;

  // /asd/*/qwe   /asd/a/a/a/a/qwe/qwe
  while (t < target.size()) {
    if (p < pattern.size() && pattern[p] != '*' && pattern[p] == target[t]) {
      ++p;
      ++t;
    } // 패턴의 위치가 *이 아니면서 같은 글자: 패턴, 타겟 한글자식 이동
    else if (p < pattern.size() && pattern[p] == '*') {
      last_star = p;
      last_match = t;
      ++p;
      ++t; // '*'는 최소 1글자 이상
    } // 패턴의 위치가 *인 상태 : 현재의 *위치 기억 및 타겟의 *의 위치 업데이트,
      // *이 최소 한글자 이상이기에 패턴, 타겟 한글자 이동
    else if (last_star != std::string::npos) {
      ++last_match;
      if (last_match >= target.size())
        return false;
      p = last_star + 1;
      t = last_match + 1;
    } // 현재 패턴의 위치가 *의 안인 경우: 타겟의 *위치 업데이트 후 타겟은
      // 한글자 상승, 패턴은 *다음 글자위치에 고정
    else {
      return false;
    }
  }

  if (p < pattern.size())
    return false;

  return true;
}

bool PathPattern::matches(const PathPattern &other) const {
  std::string pattern = this->to_string();
  std::string target = other.to_string();

  // std::cout << "\n\nmatches pattern: " << pattern << std::endl;
  // std::cout << "matches target: " << target << std::endl;
  // root의 규칙에 wildcard가 존재 하면 경우
  if (pattern.find('*') != std::string::npos)
    return wildcard_match(pattern, target);

  // root의 규칙이 /으로 되어 있는 경우
  if (!pattern.empty() && pattern[pattern.size() - 1] == '/')
    return target.find(pattern) == 0;

  // 그외에 완전히 매칭이 같아 하는 경우
  return pattern == target;
}

// Check if this pattern matches a path string
bool PathPattern::matches(const std::string &pathStr) const {
  return matches(PathPattern(pathStr));
}

// Convert PathPattern to string for debugging/display
std::string PathPattern::to_string() const {
  if (path.empty()) {
    return "";
  }
  if (path.size() == 1)
    return path[0];
  std::string result;
  for (size_t i = 0; i < path.size(); ++i) {
    if (i != 0)
      result += "/";
    if (path[i] != "/")
      result += path[i];
  }
  return result;
}

static std::size_t count_wildcards(const std::string &str) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '*')
      ++count;
  }
  return count;
}

std::string
PathPattern::extract_relative_path(const std::string &pattern,
                                   const std::string &target) const {
  if (pattern == "*") {
    if (target == "/")
      return "/";

    if (!target.empty() && target[0] == '/')
      return target.substr(1);

    return target;
  }

  if (pattern.find('*') == std::string::npos) {
    if (!pattern.empty() && pattern[pattern.size() - 1] == '/') {
      if (target.find(pattern) != 0)
        return "";
      return target.substr(pattern.size());
    }
    if (pattern == target)
      return "";
    return "";
  }

  std::size_t first_star = pattern.find('*');
  std::size_t last_star = pattern.rfind('*');

  std::string prefix = pattern.substr(0, first_star);
  std::string suffix = pattern.substr(last_star + 1);

  if (!prefix.empty()) {
    if (target.find(prefix) != 0)
      return "";
  }

  if (!suffix.empty()) {
    if (target.size() < suffix.size())
      return "";
    if (target.substr(target.size() - suffix.size()) != suffix)
      return "";
  }

  std::size_t start = prefix.size();
  std::size_t end = target.size() - suffix.size();

  if (end < start)
    return "";

  std::string result = target.substr(start, end - start);

  if (prefix.empty() && !result.empty() && result[0] == '/')
    result.erase(0, 1);

  result += suffix;
  return result;
}

bool PathPattern::extract_wildcards(const std::string &pattern,
                                    const std::string &target,
                                    std::vector<std::string> &wildcards) const {
  wildcards.clear();

  // 특수 케이스: pattern == "*"
  if (pattern == "*") {
    if (target.empty())
      return false;
    wildcards.push_back(target);
    return true;
  }

  std::vector<std::string> parts;
  std::string current;

  for (std::size_t i = 0; i < pattern.size(); ++i) {
    if (pattern[i] == '*') {
      parts.push_back(current);
      current.clear();
    } else {
      current += pattern[i];
    }
  }
  parts.push_back(current);

  bool starts_with_star = !pattern.empty() && pattern[0] == '*';
  bool ends_with_star = !pattern.empty() && pattern[pattern.size() - 1] == '*';

  std::size_t pos = 0;
  std::size_t first_literal = 0;

  if (!starts_with_star) {
    if (parts.empty())
      return false;
    if (target.find(parts[0]) != 0)
      return false;
    pos = parts[0].size();
    first_literal = 1;
  }

  for (std::size_t i = first_literal; i + 1 < parts.size(); ++i) {
    if (parts[i].empty())
      continue;

    std::size_t found = target.find(parts[i], pos);
    if (found == std::string::npos)
      return false;
    if (found == pos)
      return false; // '*'는 최소 1글자 이상

    wildcards.push_back(target.substr(pos, found - pos));
    pos = found + parts[i].size();
  }

  if (!ends_with_star) {
    const std::string &last = parts.back();

    if (target.size() < last.size())
      return false;
    if (target.substr(target.size() - last.size()) != last)
      return false;

    std::size_t end_pos = target.size() - last.size();
    if (end_pos < pos)
      return false;
    if (end_pos == pos)
      return false; // '*'는 최소 1글자 이상

    wildcards.push_back(target.substr(pos, end_pos - pos));
  } else {
    if (pos >= target.size())
      return false;
    wildcards.push_back(target.substr(pos));
  }

  return true;
}

std::string
PathPattern::apply_wildcards(const std::string &to_pattern,
                             const std::vector<std::string> &wildcards) const {
  std::string result;
  std::size_t wild_index = 0;

  for (std::size_t i = 0; i < to_pattern.size(); ++i) {
    if (to_pattern[i] == '*') {
      if (wild_index >= wildcards.size())
        return "";

      if (!result.empty() && result[result.size() - 1] == '/' &&
          !wildcards[wild_index].empty() && wildcards[wild_index][0] == '/') {
        result += wildcards[wild_index].substr(1);
      } else {
        result += wildcards[wild_index];
      }
      ++wild_index;
    } else {
      result += to_pattern[i];
    }
  }

  if (wild_index != wildcards.size())
    return "";

  return result;
}

std::string PathPattern::rewrite_path(const PathPattern &request_path,
                                      const PathPattern &to_pattern) const {
  std::string from = this->to_string();
  std::string target = request_path.to_string();
  std::string dest = to_pattern.to_string();

  // from과 dest에 있는 wildcard 수 확인
  std::size_t from_wc = count_wildcards(from);
  std::size_t dest_wc = count_wildcards(dest);

  // from에 wildcard가 존재할 때
  if (from_wc > 0) {
    // dest의 구조가 '/'이 있으면서 wildcard가 한개만 존재하는 지 확인
    // 이런 경우는 보통 실제 파일 경로 root에 request를 붙이는 용도라고 본다.
    bool looks_like_root_mapping =
        !dest.empty() && dest.find('/') != std::string::npos && dest_wc == 1;
    // root 매핑은 먼저 처리 또는
    // destination 쪽 wildcard가 1개면 relative path 사용
    bool use_relative_mapping = (looks_like_root_mapping || dest_wc == 1);

    if (use_relative_mapping) {
      // from을 기준으로 target의 wildcard원소들을 추출
      std::string relative = extract_relative_path(from, target);
      if (relative.empty() && target != from)
        return "";

      // apply_wildcards를 통해 추출한 원소들을 넣어서 만들어진 new path를 반환
      std::vector<std::string> mapped;
      mapped.push_back(relative);
      return apply_wildcards(dest, mapped);
    }

    // wildcard 개수가 같으면 캡처값 그대로 삽입
    if (from_wc == dest_wc) {
      std::vector<std::string> wildcards;
      // from을 기준으로 target의 wildcard원소들을 추출 후 wildcards에 담아서
      // 나온다.
      if (!extract_wildcards(from, target, wildcards))
        return "";
      // apply_wildcards를 통해 추출한 원소들을 넣어서 만들어진 new path를 반환
      return apply_wildcards(dest, wildcards);
    }

    return "";
  }

  // from에 '/'으로 끝나고 wildcard가 존재하지 않을 때
  // target에서 from으로 시작하지 않을시 에러.
  if (!from.empty() && from[from.size() - 1] == '/') {
    if (target.find(from) != 0)
      return "";

    // from에서 뒤 부부만 추출 ex) from = /download/, target =
    // /download/file.txt, suffix = file.txt
    std::string suffix = target.substr(from.size());

    // '/'가 중복으로 붙지 않게 new path를 생성후 반한.
    if (!dest.empty() && dest[dest.size() - 1] == '/')
      return dest + suffix;
    if (!suffix.empty() && suffix[0] == '/')
      return dest + suffix;
    return dest + "/" + suffix;
  }

  if (from == target)
    return dest;

  return "";
}