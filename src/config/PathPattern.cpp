#include "PathPattern.hpp"


bool PathPattern::wildcard_match(const std::string &pattern,
                                 const std::string &target) const {
  std::size_t p = 0;
  std::size_t t = 0;

  std::size_t last_star = std::string::npos;
  std::size_t last_match = std::string::npos;

  while (t < target.size()) {
    if (p < pattern.size() && pattern[p] != '*' && pattern[p] == target[t]) {
      ++p;
      ++t;
    } else if (p < pattern.size() && pattern[p] == '*') {
      last_star = p;
      last_match = t;
      ++p;
      ++t; // '*'는 최소 1글자 이상
    } else if (last_star != std::string::npos) {
      ++last_match;
      if (last_match >= target.size())
        return false;
      p = last_star + 1;
      t = last_match + 1;
    } else {
      return false;
    }
  }

  // target은 끝났는데 pattern이 남아 있으면 실패
  // 남은 '*'도 최소 1글자를 먹어야 하므로 실패
  if (p < pattern.size())
    return false;

  return true;
}

bool PathPattern::matches(const PathPattern &other) const {
  std::string pattern = this->to_string();
  std::string target = other.to_string();

  if (pattern.find('*') != std::string::npos)
    return wildcard_match(pattern, target);

  if (!pattern.empty() && pattern[pattern.size() - 1] == '/')
    return target.find(pattern) == 0;

  return pattern == target;
}


// Check if this pattern matches a path string
bool PathPattern::matches(const std::string &pathStr) const {
  return matches(PathPattern(pathStr));
}

// Convert PathPattern to string for debugging/display
std::string PathPattern::to_string() const {
  if (path.empty()) {
    return "/";
  }
  std::string result;
  for (size_t i = 0; i < path.size(); ++i) {
    if (path[0].find("*") == std::string::npos)
      result += "/";
    if (path[i] != "/")
      result += path[i];
  }
  return result;
}


bool PathPattern::extract_wildcards(const std::string &pattern,
                                    const std::string &target,
                                    std::vector<std::string> &wildcards) const {
  wildcards.clear();

  if (pattern == "*") {
    if (target.empty())
      return false;   // 네 규칙이 * = 1글자 이상이면
    wildcards.clear();
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

  std::size_t pos = 0;
  std::size_t part_index = 0;
  bool starts_with_star = (!pattern.empty() && pattern[0] == '*');
  bool ends_with_star = (!pattern.empty() && pattern[pattern.size() - 1] == '*');

  if (!starts_with_star) {
    if (parts.empty() || target.find(parts[0]) != 0)
      return false;
    pos = parts[0].size();
    part_index = 1;
  }

  for (; part_index + 1 < parts.size(); ++part_index) {
    std::size_t found = target.find(parts[part_index], pos);
    if (found == std::string::npos)
      return false;
    if (found == pos)
      return false; // '*'는 최소 1글자 이상

    wildcards.push_back(target.substr(pos, found - pos));
    pos = found + parts[part_index].size();
  }

  if (!ends_with_star) {
    if (parts.empty())
      return false;
    const std::string &last = parts.back();
    if (target.size() < last.size())
      return false;
    if (target.substr(target.size() - last.size()) != last)
      return false;

    std::size_t end_pos = target.size() - last.size();
    if (end_pos < pos)
      return false;
    if (end_pos == pos && pattern.find('*') != std::string::npos)
      return false;

    if (parts.size() > 1)
      wildcards.push_back(target.substr(pos, end_pos - pos));
  } else {
    if (pos >= target.size())
      return false;
    wildcards.push_back(target.substr(pos));
  }

  return true;
}

std::string PathPattern::apply_wildcards(
    const std::string &to_pattern,
    const std::vector<std::string> &wildcards) const {
  std::string result;
  std::size_t wild_index = 0;

  for (std::size_t i = 0; i < to_pattern.size(); ++i) {
    if (to_pattern[i] == '*') {
      if (wild_index >= wildcards.size())
        return "";

      if (!result.empty() && result[result.size() - 1] == '/' &&
          !wildcards[wild_index].empty() &&
          wildcards[wild_index][0] == '/') {
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

  // 1. wildcard route
  if (from.find('*') != std::string::npos) {
    std::vector<std::string> wildcards;
    if (!extract_wildcards(from, target, wildcards))
      return "";
    return apply_wildcards(dest, wildcards);
  }

  // 2. prefix route
  if (!from.empty() && from[from.size() - 1] == '/') {
    if (target.find(from) != 0)
      return "";

    std::string suffix = target.substr(from.size());

    if (!dest.empty() && dest[dest.size() - 1] == '/')
      return dest + suffix;
    return dest + "/" + suffix;
  }

  // 3. exact route
  if (from == target)
    return dest;

  return "";
}