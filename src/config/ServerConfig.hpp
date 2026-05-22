#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "RouteRule_CGI.hpp"

#define MAX_SERVER_RESPONSE_TIME 60000
#define MIN_SERVER_RESPONSE_TIME 1
#define MAX_BODY_SIZE            1048576

typedef std::map<std::string, std::map<std::string, std::string> > CGI;

enum RuleOperator {
    MULTIPLE_CHOICES,
    REDIRECT,
    FOUND,
    SEE_OTHER,
    NOT_MODIFIED,
    TEMPORARY_REDIRECT,
    PERMANENT_REDIRECT,
    AUTOINDEX,
    UPLOAD_TO,
    SERVE_FROM,
    LOGIN_USING,
    UNDEFINED,
};

struct RouteRule {
    Request::Method                     method;
    PathPattern                         path;
    RuleOperator                        op;
    PathPattern                         redirect_target;
    PathPattern                         root;
    std::string                         index;
    std::string                         auth_info;
    unsigned int                        max_body_KB;
    std::map<unsigned int, std::string> error_pages;
};

class ServerConfig {
private:
  // Final parsed values for this server block.
  std::map<std::string, std::string> header;
  unsigned int server_response_time_ms;
  std::vector<RouteRule> routes;
  std::vector<RouteRule_CGI> R_CGI;
  std::vector<std::string> file_extension;
  // Temporary state used only while parsing this server block.
  int end_flag;
  std::string err_meg;
  std::string origin_line;
  std::size_t count_line;

    Result<Void> parse_server_block(FileDescriptor& fd, char** envp);
    Result<Void> parse_header_entry(FileDescriptor&    fd,
                                    const std::string& line);
    Result<Void> parse_max_body_size(std::string line, unsigned int& maxbody);
    Result<Void> parse_route_rule_block(const std::string& method_line,
                                        FileDescriptor& fd, char** envp);
    Result<Void> create_route_rules(const std::vector<std::string>&     data,
                                    const std::vector<Request::Method>& mets,
                                    std::vector<std::size_t>& createdIndexes);
    Result<Void> apply_route_rule_entry(const std::string&        line,
                                        std::vector<std::size_t>& route_indexes,
                                        char**                    envp);
    bool         is_header_block(const std::string& line);
    bool         is_valid_server_response_time(const std::string& line);
    bool         is_path_pattern_segment(const std::string& line);
    std::vector<std::string> get_pattern_candidates(const std::string& line);
    std::vector<PathPattern>
    expand_paths_with_pattern(const std::vector<PathPattern>& paths,
                              const std::vector<std::string>& pattern,
                              std::size_t                     index);
    std::vector<PathPattern> expand_path_pattern(const std::string& line);
    bool                     has_valid_wildcard_usage(const std::string& url);
    bool                     matches_route_rule_syntax(const std::string& line);
    bool                     has_compatible_wildcards(const PathPattern& path,
                                                      const PathPattern& root);
    RuleOperator             parse_rule_operator(const std::string& indicator);

  public:
    ServerConfig(FileDescriptor&,
                 std::map<std::string, std::string>& global_cgi, char** envp);
    ServerConfig()
        : header(), server_response_time_ms(3000), routes(), R_CGI(),
          file_extension(), end_flag(0), err_meg(), origin_line(),
          count_line(0) {}

    const std::map<std::string, std::string>& get_header(void) const {
        return header;
    }
    const std::vector<std::string>& get_file_extension(void) const {
        return file_extension;
    }
    const std::string& get_err_meg(void) const { return err_meg; }
    const std::vector<RouteRule_CGI>& get_route_rule_cgi() const {
        return R_CGI;
    }
    const std::vector<RouteRule>& get_routes(void) const { return routes; }
    const std::size_t&  get_count_line(void) const { return count_line; }
    const unsigned int& get_server_response_time(void) const {
        return server_response_time_ms;
    }
    static Result<Void> apply_err_page_entry(
        const std::string& origin_line, const std::string& line,
        std::map<unsigned int, std::string>& err_map, char** envp);
    Result<RouteRule>     find_route(Request::Method    method,
                                     const std::string& path) const;
    Result<RouteRule_CGI> find_route_cgi(Request::Method    method,
                                         const std::string& path) const;
    Result<std::string>   get_rewritten_path(Request::Method    method,
                                             const std::string& path) const;
};

std::ostream &operator<<(std::ostream &os, const ServerConfig &data);

#endif
