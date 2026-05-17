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
      ++t;
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

bool PathPattern::matches(const std::string &pathStr) const {
  return matches(PathPattern(pathStr));
}

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
      return false;

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
      return false;

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

  std::size_t from_wc = count_wildcards(from);
  std::size_t dest_wc = count_wildcards(dest);

  if (from_wc > 0) {
    bool looks_like_root_mapping =
        !dest.empty() && dest.find('/') != std::string::npos && dest_wc == 1;
    bool use_relative_mapping = (looks_like_root_mapping || dest_wc == 1);

    if (use_relative_mapping) {
      std::string relative = extract_relative_path(from, target);
      if (relative.empty() && target != from)
        return "";

      std::vector<std::string> mapped;
      mapped.push_back(relative);
      return apply_wildcards(dest, mapped);
    }

    if (from_wc == dest_wc) {
      std::vector<std::string> wildcards;
      if (!extract_wildcards(from, target, wildcards))
        return "";
      return apply_wildcards(dest, wildcards);
    }

    return "";
  }

  if (!from.empty() && from[from.size() - 1] == '/') {
    if (target.find(from) != 0)
      return "";

    std::string suffix = target.substr(from.size());

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