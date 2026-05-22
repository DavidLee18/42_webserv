#ifndef WEBSERVERCONFIG_HPP
#define WEBSERVERCONFIG_HPP

#include "ServerConfig.hpp"

#define MAX_PORT_VALUE 49151
#define MIN_PORT_VALUE 1024

class WebserverConfig {
private:
  // Final parsed configuration values.
  std::string default_mime;
  std::map<std::string, std::string> global_cgi;
  std::map<std::string, std::string> type_map;
  std::map<unsigned int, ServerConfig> serverconfig_map;
  // Temporary state used only during config parsing.
  std::string origin_line;
  std::string err_meg;
  std::size_t count_line;

    Result<Void> file_parsing(FileDescriptor& file, char** envp);
    Result<Void> parse_types_block(FileDescriptor& file);
    Result<Void> parse_type_mapping(const std::string&        line,
                                    std::vector<std::string>& keys_out,
                                    std::string&              value_out);
    Result<std::vector<std::string> > parse_type_keys(const std::string& key);
    Result<Void>       is_valid_mime_type(const std::string& value);
    static bool        is_server_config_header(const std::string& line);
    Result<Void>       parse_server_config_entry(FileDescriptor&    file,
                                                 const std::string& line,
                                                 char**             envp);
    static std::string parse_server_port(const std::string& line);

    WebserverConfig(FileDescriptor& file, char** envp);

  public:
    WebserverConfig& operator=(const WebserverConfig& other) {
        if (this != &other) {
            this->err_meg          = other.err_meg;
            this->origin_line      = other.origin_line;
            this->default_mime     = other.default_mime;
            this->global_cgi       = other.global_cgi;
            this->type_map         = other.type_map;
            this->serverconfig_map = other.serverconfig_map;
            this->count_line       = other.count_line;
        }
        return *this;
    }
    const std::string& get_default_mime(void) const { return default_mime; }
    const std::map<std::string, std::string>& get_global_cgi(void) const {
        return global_cgi;
    }
    const std::map<std::string, std::string>& get_type_map(void) const {
        return type_map;
    }
    const std::map<unsigned int, ServerConfig>&
    get_serverconfig_map(void) const {
        return serverconfig_map;
    }
    static Result<WebserverConfig> parse(FileDescriptor& file, char** envp) {
        WebserverConfig temp(file, envp);

        if (temp.err_meg == "") return OK(WebserverConfig, temp);
        return ERR(WebserverConfig, temp.err_meg);
    }
};

std::ostream& operator<<(std::ostream& os, const WebserverConfig& data);

#endif
