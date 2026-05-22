#ifndef PATHPATTERN_HPP
#define PATHPATTERN_HPP

#include "ConfigError.hpp"

namespace PathPatternError {
// extract_relative_path()
const char* const RELATIVE_SOURCE_PREFIX_MISMATCH =
    "Relative path extraction failed: target path does not start with the "
    "source directory prefix";
const char* const RELATIVE_NON_WILDCARD_EXACT_MISMATCH =
    "Relative path extraction failed: non-wildcard pattern must exactly match "
    "the target path";
const char* const RELATIVE_WILDCARD_PREFIX_MISMATCH =
    "Relative path extraction failed: target path does not start with the "
    "wildcard pattern prefix";
const char* const RELATIVE_WILDCARD_SUFFIX_TOO_SHORT =
    "Relative path extraction failed: target path is shorter than the wildcard "
    "pattern suffix";
const char* const RELATIVE_WILDCARD_SUFFIX_MISMATCH =
    "Relative path extraction failed: target path does not end with the "
    "wildcard pattern suffix";
const char* const RELATIVE_INVALID_CAPTURE_RANGE =
    "Relative path extraction failed: wildcard capture range is invalid "
    "because the suffix starts before the prefix ends";
const char* const RELATIVE_EMPTY_WILDCARD_CAPTURE =
    "Relative path extraction failed: wildcard requires at least one character";
// extract_wildcards()
const char* const WILDCARD_EMPTY_TARGET =
    "Wildcard extraction failed: '*' requires at least one character, but the "
    "target path is empty";
const char* const WILDCARD_INVALID_PREFIX_LITERAL =
    "Wildcard extraction failed: pattern does not contain a valid prefix "
    "literal";
const char* const WILDCARD_PREFIX_LITERAL_MISMATCH =
    "Wildcard extraction failed: target path does not start with the required "
    "prefix literal";
const char* const WILDCARD_REQUIRED_LITERAL_NOT_FOUND =
    "Wildcard extraction failed: target path does not contain the required "
    "literal segment after the wildcard";
const char* const WILDCARD_EMPTY_BETWEEN_LITERALS =
    "Wildcard extraction failed: wildcard requires at least one character "
    "between literal segments";
const char* const WILDCARD_SUFFIX_LITERAL_TOO_SHORT =
    "Wildcard extraction failed: target path is shorter than the required "
    "suffix literal";
const char* const WILDCARD_SUFFIX_LITERAL_MISMATCH =
    "Wildcard extraction failed: target path does not end with the required "
    "suffix literal";
const char* const WILDCARD_SUFFIX_BEFORE_POSITION =
    "Wildcard extraction failed: suffix literal appears before the current "
    "wildcard search position";
const char* const WILDCARD_EMPTY_BEFORE_SUFFIX =
    "Wildcard extraction failed: wildcard requires at least one character "
    "before the suffix literal";
const char* const WILDCARD_TRAILING_EMPTY =
    "Wildcard extraction failed: trailing wildcard requires at least one "
    "character";
// apply_wildcards()
const char* const REPLACE_MORE_PLACEHOLDERS_THAN_VALUES =
    "Wildcard replacement failed: replacement pattern contains more '*' "
    "placeholders than captured wildcard values";
const char* const REPLACE_FEWER_PLACEHOLDERS_THAN_VALUES =
    "Wildcard replacement failed: replacement pattern contains fewer '*' "
    "placeholders than captured wildcard values";
// rewrite_path()
const char* const REWRITE_EMPTY_RELATIVE_SEGMENT =
    "Path rewrite failed: wildcard requires at least one character, but the "
    "request path has no relative segment";
const char* const REWRITE_WILDCARD_COUNT_MISMATCH =
    "Path rewrite failed: source pattern and replacement pattern have "
    "different wildcard counts";
const char* const REWRITE_PREFIX_MISMATCH =
    "Path rewrite failed: request path does not start with the source prefix";
const char* const REWRITE_EXACT_PATH_MISMATCH =
    "Path rewrite failed: request path does not match the source path";
} // namespace PathPatternError

class PathPattern {
  private:
    std::vector<std::string> path;

    Result<std::string>
         extract_relative_path(const std::string& route_pattern,
                               const std::string& request_path) const;
    bool wildcard_match(const std::string& pattern,
                        const std::string& target) const;
    Result<std::string>
    rewrite_with_wildcards(const std::string& route_pattern,
                           const std::string& request_path,
                           const std::string& rewrite_target,
                           std::size_t route_wc, std::size_t target_wc) const;

    Result<std::string>
    rewrite_prefix_path(const std::string& route_pattern,
                        const std::string& request_path,
                        const std::string& rewrite_target) const;

    Result<std::string>
    rewrite_exact_path(const std::string& route_pattern,
                       const std::string& request_path,
                       const std::string& rewrite_target) const;
    Result<std::string>
    replace_wildcards(const std::string&              rewrite_target,
                      const std::vector<std::string>& wildcards) const;
    Result<std::vector<std::string> >
    extract_wildcards(const std::string& route_pattern,
                      const std::string& request_path) const;

  public:
    PathPattern() : path() {}
    PathPattern(const std::string& pathStr) {
        if (pathStr == "/" || pathStr.empty()) {
            path.push_back("/");
            return;
        }
        path = utils::string_split(pathStr, "/");
        if (!pathStr.empty() && pathStr[pathStr.length() - 1] == '/')
            path.push_back("/");
        if (pathStr[0] == '/') path[0] = "/" + path[0];
    }
    PathPattern(const PathPattern& other) : path(other.path) {}

    void change_path(std::size_t i, std::string data) { path[i] = data; }
    bool is_wildcard() const { return (path.size() == 1 && path[0] == "*"); }
    bool matches(const PathPattern& other) const;
    bool matches(const std::string& pathStr) const;
    const std::vector<std::string>& get_path(void) const { return path; }
    std::string                     to_string() const;

    Result<std::string>
    rewrite_path(const PathPattern& request_path,
                 const PathPattern& rewrite_target_pattern) const;
};

std::ostream &operator<<(std::ostream &os, const PathPattern &data);

#endif
