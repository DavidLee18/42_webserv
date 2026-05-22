#include "PathPattern.hpp"

bool PathPattern::wildcard_match(const std::string& pattern,
                                 const std::string& target) const {
    std::size_t p          = 0;
    std::size_t t          = 0;

    std::size_t last_star  = std::string::npos;
    std::size_t last_match = std::string::npos;

    while (t < target.size()) {
        if (p < pattern.size() && pattern[p] != '*' &&
            pattern[p] == target[t]) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            last_star  = p;
            last_match = t;
            ++p;
            ++t;
        } else if (last_star != std::string::npos) {
            ++last_match;
            if (last_match >= target.size()) return false;
            p = last_star + 1;
            t = last_match + 1;
        } else {
            return false;
        }
    }

    if (p < pattern.size()) return false;

    return true;
}

bool PathPattern::matches(const PathPattern& other) const {
    std::string pattern = this->to_string();
    std::string target  = other.to_string();

    if (pattern.find('*') != std::string::npos)
        return wildcard_match(pattern, target);

    if (!pattern.empty() && pattern[pattern.size() - 1] == '/')
        return target.find(pattern) == 0;

    return pattern == target;
}

bool PathPattern::matches(const std::string& pathStr) const {
    return matches(PathPattern(pathStr));
}

std::string PathPattern::to_string() const {
    if (path.empty()) { return ""; }
    if (path.size() == 1) return path[0];
    std::string result;
    for (size_t i = 0; i < path.size(); ++i) {
        if (i != 0) result += "/";
        if (path[i] != "/") result += path[i];
    }
    return result;
}

static std::size_t count_wildcards(const std::string& str) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '*') ++count;
    }
    return count;
}

Result<std::string>
PathPattern::extract_relative_path(const std::string& route_pattern,
                                   const std::string& request_path) const {

    if (route_pattern == "*") {
        if (request_path == "/") return OK(std::string, "/");

        if (!request_path.empty() && request_path[0] == '/')
            return OK(std::string, request_path.substr(1));

        return OK(std::string, request_path);
    }

    if (route_pattern.find('*') == std::string::npos) {
        if (!route_pattern.empty() &&
            route_pattern[route_pattern.size() - 1] == '/') {
            if (request_path.find(route_pattern) != 0)
                return ERR(std::string,
                           PathPatternError::RELATIVE_SOURCE_PREFIX_MISMATCH);
            return OK(std::string, request_path.substr(route_pattern.size()));
        }
        if (route_pattern == request_path) return OK(std::string, "");
        return ERR(std::string,
                   PathPatternError::RELATIVE_NON_WILDCARD_EXACT_MISMATCH);
    }

    std::size_t first_star = route_pattern.find('*');
    std::size_t last_star  = route_pattern.rfind('*');

    std::string prefix     = route_pattern.substr(0, first_star);
    std::string suffix     = route_pattern.substr(last_star + 1);

    if (!prefix.empty()) {
        if (request_path.find(prefix) != 0)
            return ERR(std::string,
                       PathPatternError::RELATIVE_WILDCARD_PREFIX_MISMATCH);
    }

    if (!suffix.empty()) {
        if (request_path.size() < suffix.size())
            return ERR(std::string,
                       PathPatternError::RELATIVE_WILDCARD_SUFFIX_TOO_SHORT);
        if (request_path.substr(request_path.size() - suffix.size()) != suffix)
            return ERR(std::string,
                       PathPatternError::RELATIVE_WILDCARD_SUFFIX_MISMATCH);
    }

    std::size_t start = prefix.size();
    std::size_t end   = request_path.size() - suffix.size();

    if (end < start)
        return ERR(std::string,
                   PathPatternError::RELATIVE_INVALID_CAPTURE_RANGE);

    std::string result = request_path.substr(start, end - start);

    if (result.empty() && request_path != route_pattern)
        return ERR(std::string,
                   PathPatternError::RELATIVE_EMPTY_WILDCARD_CAPTURE);

    if (prefix.empty() && !result.empty() && result[0] == '/')
        result.erase(0, 1);

    result += suffix;
    return OK(std::string, result);
}

Result<std::vector<std::string> >
PathPattern::extract_wildcards(const std::string& route_pattern,
                               const std::string& request_path) const {
    std::vector<std::string> wildcards;

    if (route_pattern == "*") {
        if (request_path.empty())
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_EMPTY_TARGET);
        wildcards.push_back(request_path);
        return OK(std::vector<std::string>, wildcards);
    }

    std::vector<std::string> parts;
    std::string              current;

    for (std::size_t i = 0; i < route_pattern.size(); ++i) {
        if (route_pattern[i] == '*') {
            parts.push_back(current);
            current.clear();
        } else {
            current += route_pattern[i];
        }
    }
    parts.push_back(current);

    bool starts_with_star = !route_pattern.empty() && route_pattern[0] == '*';
    bool ends_with_star   = !route_pattern.empty() &&
                          route_pattern[route_pattern.size() - 1] == '*';

    std::size_t pos           = 0;
    std::size_t first_literal = 0;

    if (!starts_with_star) {
        if (parts.empty())
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_INVALID_PREFIX_LITERAL);
        if (request_path.find(parts[0]) != 0)
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_PREFIX_LITERAL_MISMATCH);
        pos           = parts[0].size();
        first_literal = 1;
    }

    for (std::size_t i = first_literal; i + 1 < parts.size(); ++i) {
        if (parts[i].empty()) continue;

        std::size_t found = request_path.find(parts[i], pos);
        if (found == std::string::npos)
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_REQUIRED_LITERAL_NOT_FOUND);
        if (found == pos)
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_EMPTY_BETWEEN_LITERALS);

        wildcards.push_back(request_path.substr(pos, found - pos));
        pos = found + parts[i].size();
    }

    if (!ends_with_star) {
        const std::string& last = parts.back();

        if (request_path.size() < last.size())
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_SUFFIX_LITERAL_TOO_SHORT);
        if (request_path.substr(request_path.size() - last.size()) != last)
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_SUFFIX_LITERAL_MISMATCH);

        std::size_t end_pos = request_path.size() - last.size();
        if (end_pos < pos)
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_SUFFIX_BEFORE_POSITION);
        if (end_pos == pos)
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_EMPTY_BEFORE_SUFFIX);

        wildcards.push_back(request_path.substr(pos, end_pos - pos));
    } else {
        if (pos >= request_path.size())
            return ERR(std::vector<std::string>,
                       PathPatternError::WILDCARD_TRAILING_EMPTY);
        wildcards.push_back(request_path.substr(pos));
    }

    return OK(std::vector<std::string>, wildcards);
}

Result<std::string> PathPattern::replace_wildcards(
    const std::string&              rewrite_target,
    const std::vector<std::string>& wildcards) const {
    std::string result;
    std::size_t wild_index = 0;

    for (std::size_t i = 0; i < rewrite_target.size(); ++i) {
        if (rewrite_target[i] == '*') {
            if (wild_index >= wildcards.size())
                return ERR(
                    std::string,
                    PathPatternError::REPLACE_MORE_PLACEHOLDERS_THAN_VALUES);

            if (!result.empty() && result[result.size() - 1] == '/' &&
                !wildcards[wild_index].empty() &&
                wildcards[wild_index][0] == '/') {
                result += wildcards[wild_index].substr(1);
            } else {
                result += wildcards[wild_index];
            }
            ++wild_index;
        } else {
            result += rewrite_target[i];
        }
    }

    if (wild_index != wildcards.size())
        return ERR(std::string,
                   PathPatternError::REPLACE_FEWER_PLACEHOLDERS_THAN_VALUES);

    return OK(std::string, result);
}

Result<std::string> PathPattern::rewrite_with_wildcards(
    const std::string& route_pattern, const std::string& request_path,
    const std::string& rewrite_target, std::size_t route_wc,
    std::size_t target_wc) const {
    std::string new_path;

    if (target_wc == 1) {
        std::string relative = "";

        TRY(std::string, std::string, relative,
            extract_relative_path(route_pattern, request_path))
        if (relative.empty() && request_path != route_pattern)
            return ERR(std::string,
                       PathPatternError::REWRITE_EMPTY_RELATIVE_SEGMENT);
        std::vector<std::string> wildcard_values;
        wildcard_values.push_back(relative);

        TRY(std::string, std::string, new_path,
            replace_wildcards(rewrite_target, wildcard_values))
        return OK(std::string, new_path);
    }

    if (route_wc == target_wc) {
        std::vector<std::string> wildcards;

        TRY(std::string, std::vector<std::string>, wildcards,
            extract_wildcards(route_pattern, request_path))
        TRY(std::string, std::string, new_path,
            replace_wildcards(rewrite_target, wildcards))
        return OK(std::string, new_path);
    }
    return ERR(std::string, PathPatternError::REWRITE_WILDCARD_COUNT_MISMATCH);
}

Result<std::string>
PathPattern::rewrite_prefix_path(const std::string& route_pattern,
                                 const std::string& request_path,
                                 const std::string& rewrite_target) const {
    if (request_path.find(route_pattern) != 0) {
        return ERR(std::string, PathPatternError::REWRITE_PREFIX_MISMATCH);
    }

    const std::string suffix = request_path.substr(route_pattern.size());

    if (!rewrite_target.empty() &&
        rewrite_target[rewrite_target.size() - 1] == '/') {
        return OK(std::string, rewrite_target + suffix);
    }

    if (!suffix.empty() && suffix[0] == '/') {
        return OK(std::string, rewrite_target + suffix);
    }

    return OK(std::string, rewrite_target + "/" + suffix);
}

Result<std::string>
PathPattern::rewrite_exact_path(const std::string& route_pattern,
                                const std::string& request_path,
                                const std::string& rewrite_target) const {
    if (route_pattern == request_path) {
        return OK(std::string, rewrite_target);
    }

    return ERR(std::string, PathPatternError::REWRITE_EXACT_PATH_MISMATCH);
}

Result<std::string>
PathPattern::rewrite_path(const PathPattern& request_pattern,
                          const PathPattern& rewrite_target_pattern) const {
    std::string route_pattern  = this->to_string();
    std::string request_path   = request_pattern.to_string();
    std::string rewrite_target = rewrite_target_pattern.to_string();

    std::size_t route_wc       = count_wildcards(route_pattern);
    std::size_t target_wc      = count_wildcards(rewrite_target);

    std::string new_path;
    if (route_wc > 0) {
        TRY(std::string, std::string, new_path,
            rewrite_with_wildcards(route_pattern, request_path, rewrite_target,
                                   route_wc, target_wc))
        return OK(std::string, new_path);
    }

    if (!route_pattern.empty() &&
        route_pattern[route_pattern.size() - 1] == '/') {
        TRY(std::string, std::string, new_path,
            rewrite_prefix_path(route_pattern, request_path, rewrite_target))
        return OK(std::string, new_path);
    }

    TRY(std::string, std::string, new_path,
        rewrite_exact_path(route_pattern, request_path, rewrite_target))
    return OK(std::string, new_path);
}

std::ostream &operator<<(std::ostream &os, const PathPattern &data) {
  os << data.to_string();
  return (os);
}