#include "PathPattern.hpp"

// bool PathPattern::segmentMatches(const std::string &pattern,
//                                  const std::string &segment) {
//   if (pattern == "*") {
//     return true;
//   }

//   if (pattern.find('*') == std::string::npos) {
//     return pattern == segment;
//   }

//   // Split pattern by * to get parts that must match
//   std::vector<std::string> parts = utils::string_split(pattern, "*");

//   size_t pos = 0;
//   for (size_t i = 0; i < parts.size(); ++i) {
//     if (parts[i].empty()) {
//       continue; // Skip empty parts from consecutive *
//     }

//     // Find this part in the segment
//     size_t found = segment.find(parts[i], pos);
//     if (found == std::string::npos) {
//       return false; // Required part not found
//     }

//     // For the first part, it should be at the beginning (unless pattern starts
//     // with *)
//     if (i == 0 && pattern[0] != '*' && found != 0) {
//       return false;
//     }

//     pos = found + parts[i].length();
//   }

//   // If pattern ends with a non-wildcard part, check that we matched to the end
//   if (!parts.empty() && !parts[parts.size() - 1].empty() &&
//       pattern[pattern.length() - 1] != '*') {
//     return pos == segment.length();
//   }

//   return true;
// }

// // Check if this pattern matches another PathPattern
// bool PathPattern::matches(const PathPattern &other) const {
//   if (is_wildcard())
//     return true;

//   if (other.is_wildcard())
//     return is_wildcard();

//   bool hasWildcard = false;
//   for (size_t i = 0; i < path.size(); ++i) {
//     if (path[i].find('*') != std::string::npos) {
//       hasWildcard = true;
//       break;
//     }
//   }

//   if (!hasWildcard && path.size() != other.path.size())
//     return false;

//   // If we have wildcards, do more flexible matching
//   if (hasWildcard) {
//     // For patterns like "*.jpg", we want to match any path ending with .jpg
//     // regardless of directory depth
//     if (path.size() == 1 && path[0].find('*') != std::string::npos) {
//       // Single segment pattern like "*.jpg"
//       // Check if any segment in other matches this pattern
//       for (size_t i = 0; i < other.path.size(); ++i) {
//         if (segmentMatches(path[0], other.path[i])) {
//           return true;
//         }
//       }
//       return false;
//     }

//     // For multi-segment patterns, match segment by segment
//     if (path.size() != other.path.size()) {
//       return false;
//     }

//     for (size_t i = 0; i < path.size(); ++i) {
//       if (!segmentMatches(path[i], other.path[i])) {
//         return false;
//       }
//     }
//     return true;
//   }

//   // No wildcards - exact match required
//   for (size_t i = 0; i < path.size(); ++i) {
//     if (path[i] != other.path[i]) {
//       return false;
//     }
//   }

//   return true;
// }






bool PathPattern::matches(const PathPattern &other) const {
  std::string pattern = this->to_string();
  std::string target = other.to_string();
  std::vector<std::string> parts = utils::string_split(pattern, "*");

  if (parts.empty())
    return true;

  if (target.find(parts[0]) != 0)
    return false;

  std::size_t search_pos = parts[0].size();

  for (std::size_t i = 1; i < parts.size(); ++i) {
    std::size_t found = target.find(parts[i], search_pos);
    if (found == std::string::npos)
      return false;
    search_pos = found + parts[i].size();
  }
  if (search_pos >= target.size())
    return false;
  return true;
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
    result += "/";
    result += path[i];
  }
  return result;
}