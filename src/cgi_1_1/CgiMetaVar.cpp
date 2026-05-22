#include "CgiMetaVar.hpp"
#include "../core/Errors.hpp"
#include "../core/utils.hpp"
#include <algorithm>
#include <limits>

CgiMetaVar::CgiMetaVar(const CgiMetaVar::Name n, const CgiMetaVar::Val v)
    : name(n), val(v) {}

CgiMetaVar::Name const& CgiMetaVar::get_name() const { return name; }
CgiMetaVar::Val const&  CgiMetaVar::get_val() const { return val; }

CgiMetaVar::CgiMetaVar(const CgiMetaVar& other) : name(other.name), val() {
    switch (name) {
    case AUTH_TYPE:
        val.auth_type = new CgiAuthType(*other.val.auth_type);
        break;
    case CONTENT_LENGTH: val.content_length = other.val.content_length; break;
    case CONTENT_TYPE:
        val.content_type = new ContentType(*other.val.content_type);
        break;
    case GATEWAY_INTERFACE:
        val.gateway_interface = other.val.gateway_interface;
        break;
    case PATH_INFO:
        val.path_info = new std::list<std::string>(*other.val.path_info);
        break;
    case PATH_TRANSLATED:
        val.path_translated = new std::string(*other.val.path_translated);
        break;
    case QUERY_STRING:
        val.query_string = new std::string(*other.val.query_string);
        break;
    case REMOTE_ADDR:
        val.remote_addr[0] = other.val.remote_addr[0];
        val.remote_addr[1] = other.val.remote_addr[1];
        val.remote_addr[2] = other.val.remote_addr[2];
        val.remote_addr[3] = other.val.remote_addr[3];
        break;
    case REMOTE_HOST:
        val.remote_host = new std::list<std::string>(*other.val.remote_host);
        break;
    case REMOTE_IDENT:
        val.remote_ident = new std::string(*other.val.remote_ident);
        break;
    case REMOTE_USER:
        val.remote_user = new std::string(*other.val.remote_user);
        break;
    case REQUEST_METHOD: val.request_method = other.val.request_method; break;
    case SCRIPT_NAME:
        val.script_name = new std::list<std::string>(*other.val.script_name);
        break;
    case SERVER_NAME:
        val.server_name = new ServerName(*other.val.server_name);
        break;
    case SERVER_PORT: val.server_port = other.val.server_port; break;
    case SERVER_PROTOCOL:
        val.server_protocol = other.val.server_protocol;
        break;
    case SERVER_SOFTWARE:
        val.server_software = other.val.server_software;
        break;
    case X_: val.etc_val = new EtcMetaVar(*other.val.etc_val); break;
    }
}

CgiMetaVar& CgiMetaVar::operator=(const CgiMetaVar& other) {
    if (this != &other) {
        switch (name) {
        case AUTH_TYPE: delete val.auth_type; break;
        case CONTENT_TYPE: delete val.content_type; break;
        case PATH_INFO: delete val.path_info; break;
        case PATH_TRANSLATED: delete val.path_translated; break;
        case QUERY_STRING: delete val.query_string; break;
        case REMOTE_HOST: delete val.remote_host; break;
        case REMOTE_IDENT: delete val.remote_ident; break;
        case REMOTE_USER: delete val.remote_user; break;
        case SCRIPT_NAME: delete val.script_name; break;
        case SERVER_NAME: delete val.server_name; break;
        case X_: delete val.etc_val; break;
        default: break;
        }

        name = other.name;
        switch (name) {
        case AUTH_TYPE:
            val.auth_type = new CgiAuthType(*other.val.auth_type);
            break;
        case CONTENT_LENGTH:
            val.content_length = other.val.content_length;
            break;
        case CONTENT_TYPE:
            val.content_type = new ContentType(*other.val.content_type);
            break;
        case GATEWAY_INTERFACE:
            val.gateway_interface = other.val.gateway_interface;
            break;
        case PATH_INFO:
            val.path_info = new std::list<std::string>(*other.val.path_info);
            break;
        case PATH_TRANSLATED:
            val.path_translated = new std::string(*other.val.path_translated);
            break;
        case QUERY_STRING:
            val.query_string = new std::string(*other.val.query_string);
            break;
        case REMOTE_ADDR:
            val.remote_addr[0] = other.val.remote_addr[0];
            val.remote_addr[1] = other.val.remote_addr[1];
            val.remote_addr[2] = other.val.remote_addr[2];
            val.remote_addr[3] = other.val.remote_addr[3];
            break;
        case REMOTE_HOST:
            val.remote_host =
                new std::list<std::string>(*other.val.remote_host);
            break;
        case REMOTE_IDENT:
            val.remote_ident = new std::string(*other.val.remote_ident);
            break;
        case REMOTE_USER:
            val.remote_user = new std::string(*other.val.remote_user);
            break;
        case REQUEST_METHOD:
            val.request_method = other.val.request_method;
            break;
        case SCRIPT_NAME:
            val.script_name =
                new std::list<std::string>(*other.val.script_name);
            break;
        case SERVER_NAME:
            val.server_name = new ServerName(*other.val.server_name);
            break;
        case SERVER_PORT: val.server_port = other.val.server_port; break;
        case SERVER_PROTOCOL:
            val.server_protocol = other.val.server_protocol;
            break;
        case SERVER_SOFTWARE:
            val.server_software = other.val.server_software;
            break;
        case X_: val.etc_val = new EtcMetaVar(*other.val.etc_val); break;
        }
    }
    return *this;
}

CgiMetaVar::~CgiMetaVar() {
    switch (name) {
    case AUTH_TYPE: delete val.auth_type; break;
    case CONTENT_TYPE: delete val.content_type; break;
    case PATH_INFO: delete val.path_info; break;
    case PATH_TRANSLATED: delete val.path_translated; break;
    case QUERY_STRING: delete val.query_string; break;
    case REMOTE_HOST: delete val.remote_host; break;
    case REMOTE_IDENT: delete val.remote_ident; break;
    case REMOTE_USER: delete val.remote_user; break;
    case SCRIPT_NAME: delete val.script_name; break;
    case SERVER_NAME: delete val.server_name; break;
    case X_: delete val.etc_val; break;
    default: break;
    }
}

CgiMetaVar CgiMetaVar::auth_type(const CgiAuthType& ty) {
    return CgiMetaVar(AUTH_TYPE,
                      (CgiMetaVar::Val){.auth_type = new CgiAuthType(ty)});
}

CgiMetaVar CgiMetaVar::content_length(const unsigned int l) {
    return CgiMetaVar(CONTENT_LENGTH, (CgiMetaVar::Val){.content_length = l});
}

CgiMetaVar CgiMetaVar::content_type(const ContentType& ty) {
    return CgiMetaVar(CONTENT_TYPE,
                      (CgiMetaVar::Val){.content_type = new ContentType(ty)});
}

CgiMetaVar CgiMetaVar::gateway_interface(const GatewayInterface i) {
    return CgiMetaVar(GATEWAY_INTERFACE,
                      (CgiMetaVar::Val){.gateway_interface = i});
}

CgiMetaVar CgiMetaVar::path_info(const std::list<std::string>& parts) {
    return CgiMetaVar(
        PATH_INFO,
        (CgiMetaVar::Val){.path_info = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::path_translated(const std::string& path) {
    return CgiMetaVar(
        PATH_TRANSLATED,
        (CgiMetaVar::Val){.path_translated = new std::string(path)});
}

CgiMetaVar CgiMetaVar::query_string(const std::string& query_string) {
    return CgiMetaVar(
        QUERY_STRING,
        (CgiMetaVar::Val){.query_string = new std::string(query_string)});
}

CgiMetaVar CgiMetaVar::remote_addr(const unsigned char a, const unsigned char b,
                                   const unsigned char c,
                                   const unsigned char d) {
    return CgiMetaVar(REMOTE_ADDR,
                      (CgiMetaVar::Val){.remote_addr = {a, b, c, d}});
}

CgiMetaVar CgiMetaVar::remote_host(const std::list<std::string>& parts) {
    return CgiMetaVar(
        REMOTE_HOST,
        (CgiMetaVar::Val){.remote_host = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::remote_ident(const std::string& id) {
    return CgiMetaVar(REMOTE_IDENT,
                      (CgiMetaVar::Val){.remote_ident = new std::string(id)});
}

CgiMetaVar CgiMetaVar::remote_user(const std::string& user) {
    return CgiMetaVar(REMOTE_USER,
                      (CgiMetaVar::Val){.remote_user = new std::string(user)});
}

CgiMetaVar CgiMetaVar::request_method(const Request::Method method) {
    return CgiMetaVar(REQUEST_METHOD,
                      (CgiMetaVar::Val){.request_method = method});
}

CgiMetaVar CgiMetaVar::script_name(const std::list<std::string>& parts) {
    return CgiMetaVar(
        SCRIPT_NAME,
        (CgiMetaVar::Val){.script_name = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::server_name(const ServerName& srv) {
    return CgiMetaVar(SERVER_NAME,
                      (CgiMetaVar::Val){.server_name = new ServerName(srv)});
}

CgiMetaVar CgiMetaVar::server_port(const unsigned short port) {
    return CgiMetaVar(SERVER_PORT, (CgiMetaVar::Val){.server_port = port});
}

CgiMetaVar CgiMetaVar::server_protocol(const ServerProtocol proto) {
    return CgiMetaVar(SERVER_PROTOCOL,
                      (CgiMetaVar::Val){.server_protocol = proto});
}

CgiMetaVar CgiMetaVar::server_software(const ServerSoftware soft) {
    return CgiMetaVar(SERVER_SOFTWARE,
                      (CgiMetaVar::Val){.server_software = soft});
}

CgiMetaVar CgiMetaVar::custom_var(const EtcMetaVar::Type ty,
                                  const std::string&     name,
                                  const std::string&     value) {
    return CgiMetaVar(
        X_, (CgiMetaVar::Val){.etc_val = new EtcMetaVar(ty, name, value)});
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_auth_type(const std::string& raw) {
    std::stringstream ss(raw);
    std::string       ty;
    std::getline(ss, ty, ' ');
    if (ss.eof() || !ss)
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    std::transform(ty.begin(), ty.end(), ty.begin(), utils::to_upper);
    if (ty == "basic")
        return OK_PAIR(CgiMetaVar, size_t,
                       CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Basic)),
                       5);
    if (ty == "digest")
        return OK_PAIR(CgiMetaVar, size_t,
                       CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Digest)),
                       6);
    return OK_PAIR(
        CgiMetaVar, size_t,
        CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::CgiAuthOther, ty)),
        ty.size());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_content_length(const std::string& raw) {
    char*               ptr = NULL;
    const char*         str = raw.c_str();
    const unsigned long l   = std::strtoul(str, &ptr, 10);
    if (ptr == NULL || *ptr != '\0' ||
        l > std::numeric_limits<unsigned int>::max())
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    return OK_PAIR(CgiMetaVar, size_t,
                   CgiMetaVar::content_length(static_cast<unsigned int>(l)),
                   ptr - str);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_content_type(std::string raw) {
    size_t       consumed  = 0;
    const size_t slash_pos = raw.find('/');
    if (slash_pos == std::string::npos)
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

    std::string type_str = raw.substr(0, slash_pos);
    std::transform(type_str.begin(), type_str.end(), type_str.begin(),
                   ::tolower);

    ContentType::Type type;
    if (type_str == "application")
        type = ContentType::application;
    else if (type_str == "audio")
        type = ContentType::audio;
    else if (type_str == "example")
        type = ContentType::example;
    else if (type_str == "font")
        type = ContentType::font;
    else if (type_str == "haptics")
        type = ContentType::haptics;
    else if (type_str == "image")
        type = ContentType::image;
    else if (type_str == "message")
        type = ContentType::message;
    else if (type_str == "model")
        type = ContentType::model;
    else if (type_str == "multipart")
        type = ContentType::multipart;
    else if (type_str == "text")
        type = ContentType::text;
    else if (type_str == "video")
        type = ContentType::video;
    else
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

    consumed                   = slash_pos + 1;

    const size_t semicolon_pos = raw.find(';', consumed);
    std::string  subtype;
    if (semicolon_pos == std::string::npos) {
        subtype  = raw.substr(consumed);
        consumed = raw.length();
    } else {
        subtype  = raw.substr(consumed, semicolon_pos - consumed);
        consumed = semicolon_pos;
    }

    // Trim whitespace from subtype
    size_t start = subtype.find_first_not_of(" \t");
    size_t end   = subtype.find_last_not_of(" \t");
    if (start == std::string::npos)
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    subtype = subtype.substr(start, end - start + 1);

    ContentType ct(type, subtype);

    // Parse parameters if present
    while (consumed < raw.length() && raw[consumed] == ';') {
        consumed++; // skip semicolon

        // Skip whitespace
        while (consumed < raw.length() &&
               (raw[consumed] == ' ' || raw[consumed] == '\t'))
            consumed++;

        if (consumed >= raw.length()) break;

        // Find parameter name
        const size_t eq_pos = raw.find('=', consumed);
        if (eq_pos == std::string::npos) break;

        std::string param_name = raw.substr(consumed, eq_pos - consumed);
        // Trim whitespace from param name
        start                  = param_name.find_first_not_of(" \t");
        end                    = param_name.find_last_not_of(" \t");
        if (start != std::string::npos)
            param_name = param_name.substr(start, end - start + 1);

        consumed = eq_pos + 1;

        // Skip whitespace after =
        while (consumed < raw.length() &&
               (raw[consumed] == ' ' || raw[consumed] == '\t'))
            consumed++;

        // Find parameter value (until semicolon or end)
        const size_t next_semi = raw.find(';', consumed);
        std::string  param_value;
        if (next_semi == std::string::npos) {
            param_value = raw.substr(consumed);
            consumed    = raw.length();
        } else {
            param_value = raw.substr(consumed, next_semi - consumed);
            consumed    = next_semi;
        }

        // Trim whitespace from param value
        start = param_value.find_first_not_of(" \t");
        end   = param_value.find_last_not_of(" \t");
        if (start != std::string::npos) {
            param_value = param_value.substr(start, end - start + 1);
            // Remove quotes if present
            if (param_value.length() >= 2 && param_value[0] == '"' &&
                param_value[param_value.length() - 1] == '"')
                param_value = param_value.substr(1, param_value.length() - 2);
        }

        ct.params[param_name] = param_value;
    }

    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::content_type(ct), consumed);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_gateway_interface(const std::string& raw) {
    std::string norm = raw;
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
    if (norm == "cgi/1.1" || norm == "cgi-1.1")
        return OK_PAIR(CgiMetaVar, size_t,
                       CgiMetaVar::gateway_interface(Cgi_1_1), raw.length());
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_path_info(const std::string& raw) {
    if (raw.empty() || raw[0] != '/')
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

    std::list<std::string> parts;
    std::stringstream      ss(raw.substr(1)); // skip leading slash
    std::string            part;

    while (std::getline(ss, part, '/')) { parts.push_back(part); }

    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::path_info(parts),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_path_translated(const std::string& raw) {
    if (raw.empty())
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::path_translated(raw),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_query_string(const std::string& raw) {
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::query_string(raw),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_addr(const std::string& raw) {
    std::stringstream          ss(raw);
    std::vector<unsigned char> octets;
    std::string                octet;

    while (std::getline(ss, octet, '.')) {
        if (octet.empty() || octet.length() > 3)
            return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

        for (size_t i = 0; i < octet.length(); i++) {
            if (octet[i] < '0' || octet[i] > '9')
                return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
        }

        char*      end = NULL;
        const long val = std::strtol(octet.c_str(), &end, 10);
        if (end == NULL || *end != '\0' || val < 0 || val > 255)
            return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

        octets.push_back(static_cast<unsigned char>(val));
    }

    if (octets.size() != 4)
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

    return OK_PAIR(
        CgiMetaVar, size_t,
        CgiMetaVar::remote_addr(octets[0], octets[1], octets[2], octets[3]),
        raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_host(const std::string& raw) {
    const Result<std::pair<ServerName, size_t> > server_res =
        ServerName::Parser::parse(raw);
    if (!server_res.error().empty())
        return ERR_PAIR(CgiMetaVar, size_t, server_res.error());

    // ServerName parser returns a ServerName, but we need a list of strings
    // For simplicity, we'll parse it as a hostname
    std::list<std::string> parts;
    std::stringstream      ss(raw);
    std::string            part;

    while (std::getline(ss, part, '.')) { parts.push_back(part); }

    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_host(parts),
                   server_res.value().second);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_ident(const std::string& raw) {
    if (raw.empty())
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_ident(raw),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_user(const std::string& raw) {
    if (raw.empty())
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_user(raw),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_request_method(const std::string& raw) {
    std::string method = raw;
    std::transform(method.begin(), method.end(), method.begin(),
                   utils::to_upper);

    Request::Method m;
    if (method == "GET")
        m = Request::GET;
    else if (method == "HEAD")
        m = Request::HEAD;
    else if (method == "OPTIONS")
        m = Request::OPTIONS;
    else if (method == "POST")
        m = Request::POST;
    else if (method == "DELETE")
        m = Request::DELETE;
    else if (method == "PUT")
        m = Request::PUT;
    else if (method == "CONNECT")
        m = Request::CONNECT;
    else if (method == "TRACE")
        m = Request::TRACE;
    else if (method == "PATCH")
        m = Request::PATCH;
    else
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::request_method(m),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_script_name(const std::string& raw) {
    if (raw.empty() || raw[0] != '/')
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

    std::list<std::string> parts;
    std::stringstream      ss(raw.substr(1)); // skip leading slash
    std::string            part;

    while (std::getline(ss, part, '/')) { parts.push_back(part); }

    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::script_name(parts),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_name(const std::string& raw) {
    const Result<std::pair<ServerName, size_t> > res =
        ServerName::Parser::parse(raw);
    if (!res.error().empty()) return ERR_PAIR(CgiMetaVar, size_t, res.error());

    return OK_PAIR(CgiMetaVar, size_t,
                   CgiMetaVar::server_name(res.value().first),
                   res.value().second);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_port(const std::string& raw) {
    char*               ptr  = NULL;
    const char*         str  = raw.c_str();
    const unsigned long port = std::strtoul(str, &ptr, 10);
    if (ptr == str || *ptr != '\0' || port > 65535)
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    return OK_PAIR(CgiMetaVar, size_t,
                   CgiMetaVar::server_port(static_cast<unsigned short>(port)),
                   raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_protocol(const std::string& raw) {
    std::string norm = raw;
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
    if (norm == "http/1.1" || norm == "http-1.1")
        return OK_PAIR(CgiMetaVar, size_t,
                       CgiMetaVar::server_protocol(Http_1_1), raw.length());
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_software(const std::string& raw) {
    std::string norm = raw;
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
    if (norm == "webserv")
        return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::server_software(Webserv),
                       raw.length());
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_custom_var(const std::string& name,
                                     const std::string& value) {
    EtcMetaVar::Type type = EtcMetaVar::Custom;
    if (name.length() >= 5 && name.substr(0, 5) == "HTTP_") {
        type = EtcMetaVar::Http;
    }
    return OK_PAIR(CgiMetaVar, size_t,
                   CgiMetaVar::custom_var(type, name, value),
                   name.length() + value.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse(std::string const& name, std::string const& value) {
    if (name == "AUTH_TYPE") return parse_auth_type(value);
    if (name == "CONTENT_LENGTH") return parse_content_length(value);
    if (name == "CONTENT_TYPE") return parse_content_type(value);
    if (name == "GATEWAY_INTERFACE") return parse_gateway_interface(value);
    if (name == "PATH_INFO") return parse_path_info(value);
    if (name == "PATH_TRANSLATED") return parse_path_translated(value);
    if (name == "QUERY_STRING") return parse_query_string(value);
    if (name == "REMOTE_ADDR") return parse_remote_addr(value);
    if (name == "REMOTE_HOST") return parse_remote_host(value);
    if (name == "REMOTE_IDENT") return parse_remote_ident(value);
    if (name == "REMOTE_USER") return parse_remote_user(value);
    if (name == "REQUEST_METHOD") return parse_request_method(value);
    if (name == "SCRIPT_NAME") return parse_script_name(value);
    if (name == "SERVER_NAME") return parse_server_name(value);
    if (name == "SERVER_PORT") return parse_server_port(value);
    if (name == "SERVER_PROTOCOL") return parse_server_protocol(value);
    if (name == "SERVER_SOFTWARE") return parse_server_software(value);
    return parse_custom_var(name, value);
}
