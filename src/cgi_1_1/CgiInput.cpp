#include "CgiInput.hpp"
#include "../core/Errors.hpp"
#include "../core/utils.hpp"

CgiInput::CgiInput() : mvars(), req_body() {}

CgiInput::CgiInput(std::vector<CgiMetaVar> const& vars, const std::string& body)
    : mvars(vars), req_body(body) {}

CgiInput::CgiInput(Request const& req) : mvars(), req_body(req.get_body()) {}

CgiInput::CgiInput(const CgiInput& other)
    : mvars(other.mvars), req_body(other.req_body) {}

CgiInput& CgiInput::operator=(const CgiInput& other) {
    if (this != &other) {
        mvars    = other.mvars;
        req_body = other.req_body;
    }
    return *this;
}

Result<CgiInput> CgiInput::Parser::parse(Request const& req) {
    CgiInput input;
    input.req_body                 = req.get_body();

    // Convert Request::Method to Request::Method for backwards compatibility
    const Request::Method h_method = req.get_method();
    input.mvars.push_back(CgiMetaVar::request_method(h_method));
    input.mvars.push_back(CgiMetaVar::server_protocol(Http_1_1));
    input.mvars.push_back(CgiMetaVar::gateway_interface(Cgi_1_1));
    input.mvars.push_back(CgiMetaVar::server_software(Webserv));

    // Parse path for SCRIPT_NAME, PATH_INFO, and QUERY_STRING
    std::string            script_path  = req.get_path();
    std::string            query_string = req.get_query();
    // Build path segments list (shared by SCRIPT_NAME and PATH_INFO)
    std::list<std::string> path_parts;
    if (!script_path.empty()) {
        std::string p =
            (script_path[0] == '/') ? script_path.substr(1) : script_path;
        std::stringstream ss(p);
        std::string       part;
        while (std::getline(ss, part, '/')) { path_parts.push_back(part); }
    }

    // Add SCRIPT_NAME
    input.mvars.push_back(CgiMetaVar::script_name(path_parts));

    // Add PATH_INFO. RFC 3875 §4.1.5 defines PATH_INFO as the extra path
    // information after the script name in the URI. Because this server does
    // not split the request URI into a script-name portion and an extra-path
    // portion, PATH_INFO is set to the same segments as SCRIPT_NAME (i.e., the
    // full request path). Scripts that rely on PATH_INFO being distinct from
    // SCRIPT_NAME will need the caller to pre-populate it via add_mvar().
    input.mvars.push_back(CgiMetaVar::path_info(path_parts));

    // Add PATH_TRANSLATED (empty — document root is not available here)
    input.mvars.push_back(CgiMetaVar::path_translated(""));

    // Add QUERY_STRING
    if (!query_string.empty()) {
        std::cerr << "[QS-DEBUG] req.get_query()=[" << query_string << "]\n";
        input.mvars.push_back(CgiMetaVar::query_string(query_string));
    }

    // Add REMOTE_ADDR (127.0.0.1 — actual client IP is not available from
    // Request::Request)
    input.mvars.push_back(CgiMetaVar::remote_addr(127, 0, 0, 1));

    // Add REMOTE_HOST (same as REMOTE_ADDR for loopback connections)
    {
        std::list<std::string> remote_host_parts;
        remote_host_parts.push_back("localhost");
        input.mvars.push_back(CgiMetaVar::remote_host(remote_host_parts));
    }

    // Add REMOTE_IDENT (empty — RFC 1413 identification not implemented)
    input.mvars.push_back(CgiMetaVar::remote_ident(""));

    // Pre-scan headers for Host and Authorization, used to populate
    // SERVER_NAME, SERVER_PORT, AUTH_TYPE, and REMOTE_USER before processing
    // all headers.
    std::map<std::string, std::string> const& headers = req.get_headers();
    std::string                               host_header_val;
    std::string                               auth_header_val;
    for (std::map<std::string, std::string>::const_iterator it =
             headers.begin();
         it != headers.end(); ++it) {
        std::string hname = it->first;
        for (size_t i = 0; i < hname.size(); i++)
            hname[i] = static_cast<char>(
                utils::to_upper(static_cast<unsigned char>(hname[i])));
        if (hname == "HOST")
            host_header_val = it->second;
        else if (hname == "AUTHORIZATION")
            auth_header_val = it->second;
    }

    // Add SERVER_NAME and SERVER_PORT (from Host header; default localhost:80)
    {
        std::string    server_name_str = "localhost";
        unsigned short server_port_val = 80;
        if (!host_header_val.empty()) {
            size_t colon_pos = host_header_val.find(':');
            if (colon_pos != std::string::npos) {
                server_name_str        = host_header_val.substr(0, colon_pos);
                std::string   port_str = host_header_val.substr(colon_pos + 1);
                char*         endptr   = NULL;
                unsigned long port =
                    std::strtoul(port_str.c_str(), &endptr, 10);
                if (endptr != NULL && endptr != port_str.c_str() &&
                    *endptr == '\0' && port > 0 && port <= 65535)
                    server_port_val = static_cast<unsigned short>(port);
            } else {
                server_name_str = host_header_val;
            }
        }
        Result<std::pair<ServerName, size_t> > sn_res =
            ServerName::Parser::parse(server_name_str);
        if (!sn_res.error().empty())
            return ERR(CgiInput, Errors::invalid_format);
        input.mvars.push_back(CgiMetaVar::server_name(sn_res.value().first));
        input.mvars.push_back(CgiMetaVar::server_port(server_port_val));
    }

    // Add AUTH_TYPE and REMOTE_USER (from Authorization header, if present)
    if (!auth_header_val.empty()) {
        size_t      sp           = auth_header_val.find(' ');
        std::string scheme       = (sp != std::string::npos)
                                     ? auth_header_val.substr(0, sp)
                                     : auth_header_val;
        std::string scheme_upper = scheme;
        for (size_t i = 0; i < scheme_upper.size(); i++)
            scheme_upper[i] = static_cast<char>(
                utils::to_upper(static_cast<unsigned char>(scheme_upper[i])));
        if (scheme_upper == "BASIC")
            input.mvars.push_back(
                CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Basic)));
        else if (scheme_upper == "DIGEST")
            input.mvars.push_back(
                CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Digest)));
        else
            input.mvars.push_back(CgiMetaVar::auth_type(
                CgiAuthType(CgiAuthType::CgiAuthOther, scheme)));
        // REMOTE_USER: username is not decoded here (would require Base64
        // decode for Basic); set to empty string
        input.mvars.push_back(CgiMetaVar::remote_user(""));
    }

    // Add HTTP headers as CGI variables
    for (std::map<std::string, std::string>::const_iterator it =
             headers.begin();
         it != headers.end(); ++it) {
        std::string header_name = it->first;

        // Convert header name to CGI format (uppercase with underscores)
        for (size_t i = 0; i < header_name.length(); i++) {
            if (header_name[i] == '-') {
                header_name[i] = '_';
            } else {
                header_name[i] = static_cast<char>(utils::to_upper(
                    static_cast<unsigned char>(header_name[i])));
            }
        }

        // Get string value (now directly a string, not Json)
        std::string value = it->second;

        // Special handling for standard CGI variables
        if (header_name == "CONTENT_TYPE") {
            Result<std::pair<CgiMetaVar, size_t> > res =
                CgiMetaVar::Parser::parse("CONTENT_TYPE", value);
            if (!res.error().empty())
                return ERR(CgiInput, Errors::invalid_format);
            input.mvars.push_back(res.value().first);
        } else if (header_name == "CONTENT_LENGTH") {
            Result<std::pair<CgiMetaVar, size_t> > res =
                CgiMetaVar::Parser::parse("CONTENT_LENGTH", value);
            if (!res.error().empty())
                return ERR(CgiInput, Errors::invalid_format);
            input.mvars.push_back(res.value().first);
        } else {
            // Add as HTTP_* variable
            input.mvars.push_back(CgiMetaVar::custom_var(
                EtcMetaVar::Http, "HTTP_" + header_name, value));
        }
    }

    // add REDIRECT_STATUS which php-cgi needs.
    input.mvars.push_back(
        CgiMetaVar::custom_var(EtcMetaVar::Custom, "REDIRECT_STATUS", "200"));

    return OK(CgiInput, input);
}

Result<Void> CgiInput::add_mvar(std::string const& name,
                                std::string const& val) {
    const Result<std::pair<CgiMetaVar, size_t> > res =
        CgiMetaVar::Parser::parse(name, val);
    if (!res.has_value()) return ERR(Void, "CgiMetaVar parse failed");
    mvars.push_back(res.value().first);
    return OKV;
}

char** CgiInput::to_envp() const {
    char** envp = new char*[mvars.size() + 1];

    for (size_t i = 0; i < mvars.size(); i++) {
        CgiMetaVar const& var = mvars[i];
        std::string       env_str;

        // Format each variable as "NAME=value"
        switch (var.get_name()) {
        case CgiMetaVar::AUTH_TYPE:
            env_str = "AUTH_TYPE=";
            if (var.get_val().auth_type->type() == CgiAuthType::Basic)
                env_str += "Basic";
            else if (var.get_val().auth_type->type() == CgiAuthType::Digest)
                env_str += "Digest";
            else if (var.get_val().auth_type->other() != NULL)
                env_str += *var.get_val().auth_type->other();
            break;

        case CgiMetaVar::CONTENT_LENGTH: {
            std::stringstream ss;
            ss << var.get_val().content_length;
            env_str = "CONTENT_LENGTH=" + ss.str();
            break;
        }

        case CgiMetaVar::CONTENT_TYPE:
            env_str = "CONTENT_TYPE=";
            if (var.get_val().content_type != NULL) {
                ContentType const& ct = *var.get_val().content_type;
                // Format type/subtype
                switch (ct.type) {
                case ContentType::application: env_str += "application/"; break;
                case ContentType::audio: env_str += "audio/"; break;
                case ContentType::example: env_str += "example/"; break;
                case ContentType::font: env_str += "font/"; break;
                case ContentType::haptics: env_str += "haptics/"; break;
                case ContentType::image: env_str += "image/"; break;
                case ContentType::message: env_str += "message/"; break;
                case ContentType::model: env_str += "model/"; break;
                case ContentType::multipart: env_str += "multipart/"; break;
                case ContentType::text: env_str += "text/"; break;
                case ContentType::video: env_str += "video/"; break;
                }
                env_str += ct.subtype;
                // Add parameters if any
                for (std::map<std::string, std::string>::const_iterator it =
                         ct.params.begin();
                     it != ct.params.end(); ++it) {
                    env_str += "; " + it->first + "=" + it->second;
                }
            }
            break;

        case CgiMetaVar::GATEWAY_INTERFACE:
            env_str = "GATEWAY_INTERFACE=CGI/1.1";
            break;

        case CgiMetaVar::PATH_INFO:
            env_str = "PATH_INFO=/";
            if (var.get_val().path_info != NULL) {
                for (std::list<std::string>::const_iterator it =
                         var.get_val().path_info->begin();
                     it != var.get_val().path_info->end(); ++it) {
                    env_str += *it + "/";
                }
                if (!var.get_val().path_info->empty()) {
                    env_str.erase(env_str.length() -
                                  1); // Remove trailing slash
                }
            }
            break;

        case CgiMetaVar::PATH_TRANSLATED:
            env_str = "PATH_TRANSLATED=";
            if (var.get_val().path_translated != NULL) {
                env_str += *var.get_val().path_translated;
            }
            break;

        case CgiMetaVar::QUERY_STRING:
            env_str = "QUERY_STRING=" + *var.get_val().query_string;
            std::cerr << "[QS-ENV] env_str=[" << env_str << "]\n";
            break;

        case CgiMetaVar::REMOTE_ADDR: {
            std::stringstream ss;
            ss << static_cast<int>(var.get_val().remote_addr[0]) << "."
               << static_cast<int>(var.get_val().remote_addr[1]) << "."
               << static_cast<int>(var.get_val().remote_addr[2]) << "."
               << static_cast<int>(var.get_val().remote_addr[3]);
            env_str = "REMOTE_ADDR=" + ss.str();
            break;
        }

        case CgiMetaVar::REMOTE_HOST:
            env_str = "REMOTE_HOST=";
            if (var.get_val().remote_host != NULL) {
                bool first = true;
                for (std::list<std::string>::const_iterator it =
                         var.get_val().remote_host->begin();
                     it != var.get_val().remote_host->end(); ++it) {
                    if (!first) env_str += ".";
                    env_str += *it;
                    first = false;
                }
            }
            break;

        case CgiMetaVar::REMOTE_IDENT:
            env_str = "REMOTE_IDENT=";
            if (var.get_val().remote_ident != NULL) {
                env_str += *var.get_val().remote_ident;
            }
            break;

        case CgiMetaVar::REMOTE_USER:
            env_str = "REMOTE_USER=";
            if (var.get_val().remote_user != NULL) {
                env_str += *var.get_val().remote_user;
            }
            break;

        case CgiMetaVar::REQUEST_METHOD:
            env_str = "REQUEST_METHOD=";
            switch (var.get_val().request_method) {
            case Request::GET: env_str += "GET"; break;
            case Request::HEAD: env_str += "HEAD"; break;
            case Request::POST: env_str += "POST"; break;
            case Request::PUT: env_str += "PUT"; break;
            case Request::DELETE: env_str += "DELETE"; break;
            case Request::OPTIONS: env_str += "OPTIONS"; break;
            case Request::CONNECT: env_str += "CONNECT"; break;
            case Request::TRACE: env_str += "TRACE"; break;
            case Request::PATCH: env_str += "PATCH"; break;
            default: env_str += "UNKNOWN"; break;
            }
            break;

        case CgiMetaVar::SCRIPT_NAME:
            env_str = "SCRIPT_NAME=/";
            if (var.get_val().script_name != NULL) {
                for (std::list<std::string>::const_iterator it =
                         var.get_val().script_name->begin();
                     it != var.get_val().script_name->end(); ++it) {
                    env_str += *it + "/";
                }
                if (!var.get_val().script_name->empty()) {
                    env_str.erase(env_str.length() -
                                  1); // Remove trailing slash
                }
            }
            break;

        case CgiMetaVar::SERVER_NAME: {
            std::ostringstream oss;
            oss << "SERVER_NAME=";
            if (var.get_val().server_name != NULL) {
                oss << var.get_val().server_name;
            }
            env_str = oss.str();
            break;
        }

        case CgiMetaVar::SERVER_PORT: {
            std::stringstream ss;
            ss << var.get_val().server_port;
            env_str = "SERVER_PORT=" + ss.str();
            break;
        }

        case CgiMetaVar::SERVER_PROTOCOL:
            env_str = "SERVER_PROTOCOL=HTTP/1.1";
            break;

        case CgiMetaVar::SERVER_SOFTWARE:
            env_str = "SERVER_SOFTWARE=webserv";
            break;

        case CgiMetaVar::X_:
            if (var.get_val().etc_val != NULL) {
                env_str = var.get_val().etc_val->get_name() + "=" +
                          var.get_val().etc_val->get_value();
            }
            break;
        }

        envp[i] = new char[env_str.length() + 1];
        std::memcpy(envp[i], env_str.c_str(), env_str.length() + 1);
    }

    envp[mvars.size()] = NULL;
    return envp;
}
