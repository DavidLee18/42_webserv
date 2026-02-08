#include "webserv.h"

// Forward declaration
unsigned char to_upper(unsigned char);

Result<Void> ContentType::add_param(std::string k, std::string v) {
  std::map<std::string, std::string>::iterator iter = params.find(k);
  if (iter == params.end())
    return ERR(Void, Errors::not_found);
  iter->second = v;
  return OKV;
}

ServerName ServerName::host(std::list<std::string> hostparts) {
  return ServerName(
      Host,
      (ServerName::Val){.host_name = new std::list<std::string>(hostparts)});
}

ServerName ServerName::ipv4(unsigned char b1, unsigned char b2,
                            unsigned char b3, unsigned char b4) {
  return ServerName(Ipv4, (ServerName::Val){.ipv4 = {b1, b2, b3, b4}});
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse_host(std::string raw) {
  std::stringstream ss(raw);
  std::list<std::string> parts;
  std::string part;
  bool dom_end = false;
  size_t j = 0;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    if (part.empty())
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    if ((!dom_end && !std::isalnum(static_cast<unsigned char>(part[0]))) || !std::isalpha(static_cast<unsigned char>(part[0])))
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    j++;
    if (part.size() > 1) {
      for (size_t i = 1; i < part.size() - 1; i++) {
        if (!std::isalnum(static_cast<unsigned char>(part[i])) && part[i] != '-')
          return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
      }
      if (!std::isalnum(static_cast<unsigned char>(part[part.size() - 1])))
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    } else {
      // Single-character part must be alphanumeric
      if (!std::isalnum(static_cast<unsigned char>(part[0])))
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    }
    j += part.size();
    std::getline(ss, part, '.');
  }
  if (!ss.eof())
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  if (part.empty())
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  if ((!dom_end && !std::isalnum(static_cast<unsigned char>(part[0]))) || !std::isalpha(static_cast<unsigned char>(part[0])))
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  j++;
  if (part.size() > 1) {
    for (size_t i = 1; i < part.size() - 1; i++) {
      if (!std::isalnum(static_cast<unsigned char>(part[i])) && part[i] != '-')
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    }
    if (!std::isalnum(static_cast<unsigned char>(part[part.size() - 1])))
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  } else {
    // Single-character part must be alphanumeric
    if (!std::isalnum(static_cast<unsigned char>(part[0])))
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  }
  return OK_PAIR(ServerName, size_t, ServerName::host(parts), j + part.size());
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse_ipv4(std::string raw) {
  std::stringstream ss(raw);
  std::vector<unsigned char> addrs;
  std::string part;
  size_t i = 0;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    if (part.size() < 1 || part.size() > 3)
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    for (size_t j = 0; j < part.size(); j++) {
      if (part[j] < '0' || part[j] > '9')
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    }
    i += part.size();
    addrs.push_back(static_cast<unsigned char>(std::atoi(part.c_str())));
    std::getline(ss, part, '.');
  }
  if (!ss.eof())
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  if (part.size() < 1 || part.size() > 3)
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  for (size_t j = 0; j < part.size(); j++) {
    if (part[j] < '0' || part[j] > '9')
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  }
  addrs.push_back(static_cast<unsigned char>(std::atoi(part.c_str())));
  if (addrs.size() != 4)
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  return OK_PAIR(ServerName, size_t,
                 ServerName::ipv4(addrs[0], addrs[1], addrs[2], addrs[3]),
                 i + part.size());
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse(std::string raw) {
  Result<std::pair<ServerName, size_t> > res = parse_host(raw);
  if (res.error().empty())
    return res;
  return parse_ipv4(raw);
}

CgiMetaVar CgiMetaVar::auth_type(CgiAuthType ty) {
  return CgiMetaVar(AUTH_TYPE,
                    (CgiMetaVar::Val){.auth_type = new CgiAuthType(ty)});
}

CgiMetaVar CgiMetaVar::content_length(unsigned int l) {
  return CgiMetaVar(CONTENT_LENGTH, (CgiMetaVar::Val){.content_length = l});
}

CgiMetaVar CgiMetaVar::content_type(ContentType ty) {
  return CgiMetaVar(CONTENT_TYPE,
                    (CgiMetaVar::Val){.content_type = new ContentType(ty)});
}

CgiMetaVar CgiMetaVar::gateway_interface(GatewayInterface i) {
  return CgiMetaVar(GATEWAY_INTERFACE,
                    (CgiMetaVar::Val){.gateway_interface = i});
}

CgiMetaVar CgiMetaVar::path_info(std::list<std::string> parts) {
  return CgiMetaVar(
      PATH_INFO,
      (CgiMetaVar::Val){.path_info = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::path_translated(std::string path) {
  return CgiMetaVar(
      PATH_TRANSLATED,
      (CgiMetaVar::Val){.path_translated = new std::string(path)});
}

CgiMetaVar
CgiMetaVar::query_string(std::map<std::string, std::string> query_map) {
  return CgiMetaVar(
      QUERY_STRING,
      (CgiMetaVar::Val){.query_string =
                            new std::map<std::string, std::string>(query_map)});
}

CgiMetaVar CgiMetaVar::remote_addr(unsigned char a, unsigned char b,
                                   unsigned char c, unsigned char d) {
  return CgiMetaVar(REMOTE_ADDR,
                    (CgiMetaVar::Val){.remote_addr = {a, b, c, d}});
}

CgiMetaVar CgiMetaVar::remote_host(std::list<std::string> parts) {
  return CgiMetaVar(
      REMOTE_HOST,
      (CgiMetaVar::Val){.remote_host = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::remote_ident(std::string id) {
  return CgiMetaVar(REMOTE_IDENT,
                    (CgiMetaVar::Val){.remote_ident = new std::string(id)});
}

CgiMetaVar CgiMetaVar::remote_user(std::string user) {
  return CgiMetaVar(REMOTE_USER,
                    (CgiMetaVar::Val){.remote_user = new std::string(user)});
}

CgiMetaVar CgiMetaVar::request_method(Http::Method method) {
  return CgiMetaVar(REQUEST_METHOD,
                    (CgiMetaVar::Val){.request_method = method});
}

CgiMetaVar CgiMetaVar::script_name(std::list<std::string> parts) {
  return CgiMetaVar(
      SCRIPT_NAME,
      (CgiMetaVar::Val){.script_name = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::server_name(ServerName srv) {
  return CgiMetaVar(SERVER_NAME,
                    (CgiMetaVar::Val){.server_name = new ServerName(srv)});
}

CgiMetaVar CgiMetaVar::server_port(unsigned short port) {
  return CgiMetaVar(SERVER_PORT, (CgiMetaVar::Val){.server_port = port});
}

CgiMetaVar CgiMetaVar::server_protocol(ServerProtocol proto) {
  return CgiMetaVar(SERVER_PROTOCOL,
                    (CgiMetaVar::Val){.server_protocol = proto});
}

CgiMetaVar CgiMetaVar::server_software(ServerSoftware soft) {
  return CgiMetaVar(SERVER_SOFTWARE,
                    (CgiMetaVar::Val){.server_software = soft});
}

CgiMetaVar CgiMetaVar::custom_var(EtcMetaVar::Type ty, std::string name,
                                  std::string value) {
  return CgiMetaVar(
      X_, (CgiMetaVar::Val){.etc_val = new EtcMetaVar(ty, name, value)});
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_auth_type(std::string raw) {
  std::stringstream ss(raw);
  std::string ty;
  std::getline(ss, ty, ' ');
  if (ss.eof() || !ss)
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  std::transform(ty.begin(), ty.end(), ty.begin(), to_upper);
  if (ty == "basic")
    return OK_PAIR(CgiMetaVar, size_t,
                   CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Basic)), 5);
  if (ty == "digest")
    return OK_PAIR(CgiMetaVar, size_t,
                   CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Digest)), 6);
  return OK_PAIR(
      CgiMetaVar, size_t,
      CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::CgiAuthOther, ty)),
      ty.size());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_content_length(std::string raw) {
  char *ptr = NULL;
  const char *str = raw.c_str();
  unsigned long l = std::strtoul(str, &ptr, 10);
  if (ptr == NULL || *ptr != '\0' || l > UINT32_MAX)
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t,
                 CgiMetaVar::content_length(static_cast<unsigned int>(l)),
                 ptr - str);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_content_type(std::string raw) {
  size_t consumed = 0;
  size_t slash_pos = raw.find('/');
  if (slash_pos == std::string::npos)
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

  std::string type_str = raw.substr(0, slash_pos);
  // Convert to lowercase for comparison
  std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::tolower);

  // Parse main type
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

  consumed = slash_pos + 1;

  // Parse subtype (everything until semicolon or end of string)
  size_t semicolon_pos = raw.find(';', consumed);
  std::string subtype;
  if (semicolon_pos == std::string::npos) {
    subtype = raw.substr(consumed);
    consumed = raw.length();
  } else {
    subtype = raw.substr(consumed, semicolon_pos - consumed);
    consumed = semicolon_pos;
  }

  // Trim whitespace from subtype
  size_t start = subtype.find_first_not_of(" \t");
  size_t end = subtype.find_last_not_of(" \t");
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

    if (consumed >= raw.length())
      break;

    // Find parameter name
    size_t eq_pos = raw.find('=', consumed);
    if (eq_pos == std::string::npos)
      break;

    std::string param_name = raw.substr(consumed, eq_pos - consumed);
    // Trim whitespace from param name
    start = param_name.find_first_not_of(" \t");
    end = param_name.find_last_not_of(" \t");
    if (start != std::string::npos)
      param_name = param_name.substr(start, end - start + 1);

    consumed = eq_pos + 1;

    // Skip whitespace after =
    while (consumed < raw.length() &&
           (raw[consumed] == ' ' || raw[consumed] == '\t'))
      consumed++;

    // Find parameter value (until semicolon or end)
    size_t next_semi = raw.find(';', consumed);
    std::string param_value;
    if (next_semi == std::string::npos) {
      param_value = raw.substr(consumed);
      consumed = raw.length();
    } else {
      param_value = raw.substr(consumed, next_semi - consumed);
      consumed = next_semi;
    }

    // Trim whitespace from param value
    start = param_value.find_first_not_of(" \t");
    end = param_value.find_last_not_of(" \t");
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
CgiMetaVar::Parser::parse_gateway_interface(std::string raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "cgi/1.1" || norm == "cgi-1.1")
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::gateway_interface(Cgi_1_1),
                   raw.length());
  return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_path_info(std::string raw) {
  if (raw.empty() || raw[0] != '/')
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

  std::list<std::string> parts;
  std::stringstream ss(raw.substr(1)); // skip leading slash
  std::string part;

  while (std::getline(ss, part, '/')) {
    parts.push_back(part);
  }

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::path_info(parts),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_path_translated(std::string raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::path_translated(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_query_string(std::string raw) {
  std::map<std::string, std::string> query_map;

  if (raw.empty()) {
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::query_string(query_map), 0);
  }

  std::stringstream ss(raw);
  std::string pair;

  while (std::getline(ss, pair, '&')) {
    size_t eq_pos = pair.find('=');
    if (eq_pos == std::string::npos) {
      query_map[pair] = "";
    } else {
      std::string key = pair.substr(0, eq_pos);
      std::string value = pair.substr(eq_pos + 1);
      query_map[key] = value;
    }
  }

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::query_string(query_map),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_addr(std::string raw) {
  std::stringstream ss(raw);
  std::vector<unsigned char> octets;
  std::string octet;

  while (std::getline(ss, octet, '.')) {
    if (octet.empty() || octet.length() > 3)
      return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

    for (size_t i = 0; i < octet.length(); i++) {
      if (octet[i] < '0' || octet[i] > '9')
        return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
    }

    long val = std::atol(octet.c_str());
    if (val < 0 || val > 255)
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
CgiMetaVar::Parser::parse_remote_host(std::string raw) {
  Result<std::pair<ServerName, size_t> > server_res =
      ServerName::Parser::parse(raw);
  if (!server_res.error().empty())
    return ERR_PAIR(CgiMetaVar, size_t, server_res.error());

  // ServerName parser returns a ServerName, but we need a list of strings
  // For simplicity, we'll parse it as a hostname
  std::list<std::string> parts;
  std::stringstream ss(raw);
  std::string part;

  while (std::getline(ss, part, '.')) {
    parts.push_back(part);
  }

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_host(parts),
                 server_res.value().second);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_ident(std::string raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_ident(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_user(std::string raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_user(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_request_method(std::string raw) {
  std::string method = raw;
  std::transform(method.begin(), method.end(), method.begin(), to_upper);

  Http::Method m;
  if (method == "GET")
    m = Http::GET;
  else if (method == "HEAD")
    m = Http::HEAD;
  else if (method == "OPTIONS")
    m = Http::OPTIONS;
  else if (method == "POST")
    m = Http::POST;
  else if (method == "DELETE")
    m = Http::DELETE;
  else if (method == "PUT")
    m = Http::PUT;
  else if (method == "CONNECT")
    m = Http::CONNECT;
  else if (method == "TRACE")
    m = Http::TRACE;
  else if (method == "PATCH")
    m = Http::PATCH;
  else
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::request_method(m),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_script_name(std::string raw) {
  if (raw.empty() || raw[0] != '/')
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

  std::list<std::string> parts;
  std::stringstream ss(raw.substr(1)); // skip leading slash
  std::string part;

  while (std::getline(ss, part, '/')) {
    parts.push_back(part);
  }

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::script_name(parts),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_name(std::string raw) {
  Result<std::pair<ServerName, size_t> > res = ServerName::Parser::parse(raw);
  if (!res.error().empty())
    return ERR_PAIR(CgiMetaVar, size_t, res.error());

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::server_name(res.value().first),
                 res.value().second);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_port(std::string raw) {
  char *ptr = NULL;
  const char *str = raw.c_str();
  unsigned long port = std::strtoul(str, &ptr, 10);
  if (ptr == str || *ptr != '\0' || port > 65535)
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t,
                 CgiMetaVar::server_port(static_cast<unsigned short>(port)),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_protocol(std::string raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "http/1.1" || norm == "http-1.1")
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::server_protocol(Http_1_1),
                   raw.length());
  return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_software(std::string raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "webserv")
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::server_software(Webserv),
                   raw.length());
  return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_custom_var(std::string name, std::string value) {
  EtcMetaVar::Type type = EtcMetaVar::Custom;
  if (name.length() >= 5 && name.substr(0, 5) == "HTTP_") {
    type = EtcMetaVar::Http;
  }
  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::custom_var(type, name, value),
                 name.length() + value.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse(std::string const &name, std::string const &value) {
  if (name == "AUTH_TYPE")
    return parse_auth_type(value);
  if (name == "CONTENT_LENGTH")
    return parse_content_length(value);
  if (name == "CONTENT_TYPE")
    return parse_content_type(value);
  if (name == "GATEWAY_INTERFACE")
    return parse_gateway_interface(value);
  if (name == "PATH_INFO")
    return parse_path_info(value);
  if (name == "PATH_TRANSLATED")
    return parse_path_translated(value);
  if (name == "QUERY_STRING")
    return parse_query_string(value);
  if (name == "REMOTE_ADDR")
    return parse_remote_addr(value);
  if (name == "REMOTE_HOST")
    return parse_remote_host(value);
  if (name == "REMOTE_IDENT")
    return parse_remote_ident(value);
  if (name == "REMOTE_USER")
    return parse_remote_user(value);
  if (name == "REQUEST_METHOD")
    return parse_request_method(value);
  if (name == "SCRIPT_NAME")
    return parse_script_name(value);
  if (name == "SERVER_NAME")
    return parse_server_name(value);
  if (name == "SERVER_PORT")
    return parse_server_port(value);
  if (name == "SERVER_PROTOCOL")
    return parse_server_protocol(value);
  if (name == "SERVER_SOFTWARE")
    return parse_server_software(value);
  return parse_custom_var(name, value);
}

// CgiInput constructors
CgiInput::CgiInput()
    : mvars(), req_body(Http::Body::Empty, (Http::Body::Value){._null = NULL}) {
}

CgiInput::CgiInput(std::vector<CgiMetaVar> vars, Http::Body body)
    : mvars(vars), req_body(body) {}

CgiInput::CgiInput(Http::Request const &req) : mvars(), req_body(req.body()) {}

Result<CgiInput> CgiInput::Parser::parse(Http::Request const &req) {
  CgiInput input;
  input.req_body = req.body();

  // Add REQUEST_METHOD
  CgiMetaVar method_var = CgiMetaVar::request_method(req.method());
  input.mvars.push_back(method_var);

  // Add SERVER_PROTOCOL
  CgiMetaVar protocol_var = CgiMetaVar::server_protocol(Http_1_1);
  input.mvars.push_back(protocol_var);

  // Add GATEWAY_INTERFACE
  CgiMetaVar gateway_var = CgiMetaVar::gateway_interface(Cgi_1_1);
  input.mvars.push_back(gateway_var);

  // Parse path for SCRIPT_NAME and QUERY_STRING
  std::string path = req.path();
  size_t query_pos = path.find('?');
  std::string script_path;
  std::string query_string;

  if (query_pos != std::string::npos) {
    script_path = path.substr(0, query_pos);
    query_string = path.substr(query_pos + 1);
  } else {
    script_path = path;
    query_string = "";
  }

  // Add SCRIPT_NAME
  if (!script_path.empty()) {
    std::list<std::string> script_parts;
    if (script_path[0] == '/') {
      std::stringstream ss(script_path.substr(1));
      std::string part;
      while (std::getline(ss, part, '/')) {
        script_parts.push_back(part);
      }
    }
    CgiMetaVar script_var = CgiMetaVar::script_name(script_parts);
    input.mvars.push_back(script_var);
  }

  // Add QUERY_STRING
  if (!query_string.empty()) {
    std::map<std::string, std::string> query_map;
    std::stringstream ss(query_string);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
      size_t eq_pos = pair.find('=');
      if (eq_pos != std::string::npos) {
        query_map[pair.substr(0, eq_pos)] = pair.substr(eq_pos + 1);
      } else {
        query_map[pair] = "";
      }
    }
    CgiMetaVar query_var = CgiMetaVar::query_string(query_map);
    input.mvars.push_back(query_var);
  }

  // Add HTTP headers as CGI variables
  std::map<std::string, std::string> const &headers = req.headers();
  for (std::map<std::string, std::string>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    std::string header_name = it->first;

    // Convert header name to CGI format (uppercase with underscores)
    for (size_t i = 0; i < header_name.length(); i++) {
      if (header_name[i] == '-') {
        header_name[i] = '_';
      } else {
        header_name[i] = static_cast<char>(
            to_upper(static_cast<unsigned char>(header_name[i])));
      }
    }

    // Get string value (now directly a string, not Json)
    std::string value = it->second;

    // Special handling for standard CGI variables
    if (header_name == "CONTENT_TYPE") {
      Result<std::pair<CgiMetaVar, size_t> > res =
          CgiMetaVar::Parser::parse("CONTENT_TYPE", value);
      if (res.error().empty()) {
        input.mvars.push_back(res.value().first);
      }
    } else if (header_name == "CONTENT_LENGTH") {
      Result<std::pair<CgiMetaVar, size_t> > res =
          CgiMetaVar::Parser::parse("CONTENT_LENGTH", value);
      if (res.error().empty()) {
        input.mvars.push_back(res.value().first);
      }
    } else {
      // Add as HTTP_* variable
      std::string cgi_name = "HTTP_" + header_name;
      CgiMetaVar custom_var =
          CgiMetaVar::custom_var(EtcMetaVar::Http, cgi_name, value);
      input.mvars.push_back(custom_var);
    }
  }

  return OK(CgiInput, input);
}

char **CgiInput::to_envp() const {
  // Allocate array with space for all variables plus NULL terminator
  char **envp = new char *[mvars.size() + 1];

  for (size_t i = 0; i < mvars.size(); i++) {
    CgiMetaVar const &var = mvars[i];
    std::string env_str;

    // Format each variable as "NAME=value"
    switch (var.get_name()) {
    case CgiMetaVar::AUTH_TYPE:
      env_str = "AUTH_TYPE=";
      if (var.get_val().auth_type->type() == CgiAuthType::Basic) {
        env_str += "Basic";
      } else if (var.get_val().auth_type->type() == CgiAuthType::Digest) {
        env_str += "Digest";
      } else if (var.get_val().auth_type->other() != NULL) {
        env_str += *var.get_val().auth_type->other();
      }
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
        ContentType const &ct = *var.get_val().content_type;
        // Format type/subtype
        switch (ct.type) {
        case ContentType::application:
          env_str += "application/";
          break;
        case ContentType::audio:
          env_str += "audio/";
          break;
        case ContentType::example:
          env_str += "example/";
          break;
        case ContentType::font:
          env_str += "font/";
          break;
        case ContentType::haptics:
          env_str += "haptics/";
          break;
        case ContentType::image:
          env_str += "image/";
          break;
        case ContentType::message:
          env_str += "message/";
          break;
        case ContentType::model:
          env_str += "model/";
          break;
        case ContentType::multipart:
          env_str += "multipart/";
          break;
        case ContentType::text:
          env_str += "text/";
          break;
        case ContentType::video:
          env_str += "video/";
          break;
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
          env_str.erase(env_str.length() - 1); // Remove trailing slash
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
      env_str = "QUERY_STRING=";
      if (var.get_val().query_string != NULL) {
        bool first = true;
        for (std::map<std::string, std::string>::const_iterator it =
                 var.get_val().query_string->begin();
             it != var.get_val().query_string->end(); ++it) {
          if (!first)
            env_str += "&";
          env_str += it->first + "=" + it->second;
          first = false;
        }
      }
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
          if (!first)
            env_str += ".";
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
      case Http::GET:
        env_str += "GET";
        break;
      case Http::HEAD:
        env_str += "HEAD";
        break;
      case Http::POST:
        env_str += "POST";
        break;
      case Http::PUT:
        env_str += "PUT";
        break;
      case Http::DELETE:
        env_str += "DELETE";
        break;
      case Http::OPTIONS:
        env_str += "OPTIONS";
        break;
      case Http::CONNECT:
        env_str += "CONNECT";
        break;
      case Http::TRACE:
        env_str += "TRACE";
        break;
      case Http::PATCH:
        env_str += "PATCH";
        break;
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
          env_str.erase(env_str.length() - 1); // Remove trailing slash
        }
      }
      break;

    case CgiMetaVar::SERVER_NAME:
      env_str = "SERVER_NAME=";
      if (var.get_val().server_name != NULL) {
        // ServerName formatting - could be host or IPv4
        // For simplicity, we'll format it as a string
        // This is a simplified implementation
        env_str += "localhost"; // TODO: proper ServerName formatting
      }
      break;

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

    // Allocate and copy the string
    envp[i] = new char[env_str.length() + 1];
    std::strcpy(envp[i], env_str.c_str());
  }

  // NULL terminate the array
  envp[mvars.size()] = NULL;

  return envp;
}

unsigned char to_upper(unsigned char c) {
  return static_cast<unsigned char>(std::toupper(static_cast<int>(c)));
}

// CgiDelegate implementation

// CGI read timeout constants
// Initial timeout for first read
#define CGI_INITIAL_TIMEOUT_SEC 30
// Timeout for subsequent reads
#define CGI_SUBSEQUENT_TIMEOUT_SEC 5

CgiDelegate::CgiDelegate(const Http::Request &req, const std::string &script)
    : env(), script_path(script), request(req) {
  // Parse the HTTP request to CgiInput
  Result<CgiInput> parse_result = CgiInput::Parser::parse(req);
  if (parse_result.error().empty()) {
    env = parse_result.value();
  }
}

Result<Http::Response> CgiDelegate::execute(int timeout_ms, EPoll *epoll) {

  if (epoll == NULL) {
    return ERR(Http::Response, "EPoll instance required");
  }

  // Create pipes for communication
  int stdin_pipe[2];
  int stdout_pipe[2];

  if (pipe(stdin_pipe) == -1) {
    return ERR(Http::Response, "Failed to create stdin pipe");
  }
  if (pipe(stdout_pipe) == -1) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    return ERR(Http::Response, "Failed to create stdout pipe");
  }

  // Fork the process
  pid_t pid = fork();
  if (pid == -1) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return ERR(Http::Response, "Failed to fork process");
  }

  if (pid == 0) {
    // Child process

    // Close unused pipe ends
    close(stdin_pipe[1]);  // Close write end of stdin pipe
    close(stdout_pipe[0]); // Close read end of stdout pipe

    // Redirect stdin and stdout
    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
      std::cerr << "Failed to redirect stdin" << std::endl;
      exit(1);
    }
    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
      std::cerr << "Failed to redirect stdout" << std::endl;
      exit(1);
    }

    // Close the pipe file descriptors after duplication
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Prepare environment variables
    char **envp = env.to_envp();

    // Prepare arguments
    char *argv[2];
    argv[0] = const_cast<char *>(script_path.c_str());
    argv[1] = NULL;

    // Execute the CGI script
    execve(script_path.c_str(), argv, envp);

    // If execve returns, it failed
    // Clean up allocated memory before exit
    for (size_t i = 0; envp[i] != NULL; i++) {
      delete[] envp[i];
    }
    delete[] envp;

    std::cerr << "Failed to execute CGI script: " << script_path << std::endl;
    exit(1);
  }

  // Parent process

  // Close unused pipe ends
  close(stdin_pipe[0]);  // Close read end of stdin pipe
  close(stdout_pipe[1]); // Close write end of stdout pipe

  // Create FileDescriptor objects for the pipes
  Result<FileDescriptor> stdin_fd_res = FileDescriptor::from_raw(stdin_pipe[1]);
  if (!stdin_fd_res.error().empty()) {
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return ERR(Http::Response, "Failed to create stdin FileDescriptor");
  }
  FileDescriptor stdin_fd = stdin_fd_res.value();

  Result<FileDescriptor> stdout_fd_res =
      FileDescriptor::from_raw(stdout_pipe[0]);
  if (!stdout_fd_res.error().empty()) {
    // stdin_pipe[1] is owned by stdin_fd, will be closed automatically
    close(stdout_pipe[0]);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return ERR(Http::Response, "Failed to create stdout FileDescriptor");
  }
  FileDescriptor stdout_fd = stdout_fd_res.value();

  // Prepare request body for writing
  const Http::Body &body = request.body();
  std::string body_str;

  switch (body.type()) {
  case Http::Body::Html:
    if (body.value().html_raw != NULL) {
      body_str = *body.value().html_raw;
    }
    break;

  case Http::Body::HttpJson:
    if (body.value().json != NULL) {
      std::stringstream ss;
      Json json_copy = *body.value().json;
      ss << json_copy;
      body_str = ss.str();
    }
    break;

  case Http::Body::HttpFormUrlEncoded:
    if (body.value().form != NULL) {
      std::stringstream ss;
      const std::map<std::string, std::string> &form = *body.value().form;
      bool first = true;
      for (std::map<std::string, std::string>::const_iterator it = form.begin();
           it != form.end(); ++it) {
        if (!first) {
          ss << "&";
        }
        ss << it->first << "=" << it->second;
        first = false;
      }
      body_str = ss.str();
    }
    break;

  case Http::Body::Empty:
    // No body to write
    break;
  }

  // Write request body to CGI stdin if present, using EPoll to check
  // writability
  if (!body_str.empty()) {
    // Add stdin_fd to epoll for writing
    const FileDescriptor *stdin_fd_ptr =
        const_cast<const FileDescriptor *>(&stdin_fd);
    Event write_event(stdin_fd_ptr, false, true, false, false, false, false);
    Option write_option(false, false, false, false);
    Result<const FileDescriptor *> add_result =
        epoll->add_fd(stdin_fd, write_event, write_option);

    if (!add_result.error().empty()) {
      // FileDescriptor destructors will close the pipes
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response, "Failed to add stdin to epoll");
    }

    size_t total_written = 0;
    while (total_written < body_str.length()) {
      // Wait for the fd to be writable
      Result<Events> wait_result = epoll->wait(timeout_ms);
      if (!wait_result.error().empty()) {
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response, "EPoll wait failed for stdin");
      }

      Events events = wait_result.value();

      // Check if timeout occurred (no events returned)
      if (events.is_end()) {
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response, "Timeout waiting for stdin writability");
      }

      bool fd_ready = false;

      for (; !events.is_end(); ++events) {
        Result<const Event *> event_result = *events;
        if (!event_result.error().empty()) {
          continue;
        }
        const Event *ev = event_result.value();
        if (*ev->fd == stdin_pipe[1] && ev->out) {
          fd_ready = true;
          break;
        }
      }

      if (!fd_ready) {
        // Got events but not for our fd, continue waiting
        continue;
      }

      // FD is ready, perform write
      ssize_t written = write(stdin_pipe[1], body_str.c_str() + total_written,
                              body_str.length() - total_written);
      if (written < 0) {
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response, "Failed to write to CGI stdin");
      } else if (written == 0) {
        // Pipe closed by reader (child process)
        epoll->del_fd(stdin_fd);
        // FileDescriptor destructors will close the pipes
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(Http::Response, "CGI process closed stdin prematurely");
      }
      total_written += static_cast<size_t>(written);
    }

    // Remove stdin from epoll and close it
    epoll->del_fd(stdin_fd);
  }
  // Close stdin by letting stdin_fd go out of scope
  // (Manual close would be a double-close bug)
  {
    // Create a scope to destroy stdin_fd and close stdin_pipe[1]
    FileDescriptor temp_fd = stdin_fd;
    // temp_fd destructor will close stdin_pipe[1]
  }

  // Add stdout_fd to epoll for reading
  const FileDescriptor *stdout_fd_ptr =
      const_cast<const FileDescriptor *>(&stdout_fd);
  Event read_event(stdout_fd_ptr, true, false, false, false, false, false);
  Option read_option(false, false, false, false);
  Result<const FileDescriptor *> add_stdout_result =
      epoll->add_fd(stdout_fd, read_event, read_option);

  if (!add_stdout_result.error().empty()) {
    // stdout_fd destructor will close stdout_pipe[0]
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return ERR(Http::Response, "Failed to add stdout to epoll");
  }

  // Read output using EPoll to check readability
  std::string output;
  char buffer[4096];

  while (true) {
    // Wait for data to be available
    Result<Events> wait_result = epoll->wait(timeout_ms);
    if (!wait_result.error().empty()) {
      epoll->del_fd(stdout_fd);
      // stdout_fd destructor will close stdout_pipe[0]
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response, "EPoll wait failed for stdout");
    }

    Events events = wait_result.value();

    // Check if timeout occurred (no events returned)
    if (events.is_end()) {
      epoll->del_fd(stdout_fd);
      // stdout_fd destructor will close stdout_pipe[0]
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response, "CGI execution timeout");
    }

    bool fd_ready = false;

    for (; !events.is_end(); ++events) {
      Result<const Event *> event_result = *events;
      if (!event_result.error().empty()) {
        continue;
      }
      const Event *ev = event_result.value();
      if (*ev->fd == stdout_pipe[0] && ev->in) {
        fd_ready = true;
        break;
      }
    }

    if (!fd_ready) {
      // Got events but not for our fd, continue waiting
      continue;
    }

    // FD is ready, perform read
    ssize_t bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer));

    if (bytes_read > 0) {
      output.append(buffer, static_cast<size_t>(bytes_read));
    } else if (bytes_read == 0) {
      // EOF reached
      break;
    } else {
      // Read error
      epoll->del_fd(stdout_fd);
      // stdout_fd destructor will close stdout_pipe[0]
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ERR(Http::Response, "Failed to read from CGI stdout");
    }
  }

  // Remove stdout from epoll and close it
  epoll->del_fd(stdout_fd);
  // stdout_fd destructor will close stdout_pipe[0] when it goes out of scope

  // Wait for child process
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    return ERR(Http::Response, "Failed to wait for child process");
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return ERR(Http::Response, "CGI script failed");
  }

  // Parse CGI output to extract headers and body
  // CGI scripts output headers followed by blank line, then body
  std::string headers_section;
  std::string body_section;
  size_t blank_line_pos = output.find("\r\n\r\n");

  if (blank_line_pos == std::string::npos) {
    blank_line_pos = output.find("\n\n");
    if (blank_line_pos != std::string::npos) {
      headers_section = output.substr(0, blank_line_pos);
      body_section = output.substr(blank_line_pos + 2);
    } else {
      // No headers separator found, treat all as body
      body_section = output;
    }
  } else {
    headers_section = output.substr(0, blank_line_pos);
    body_section = output.substr(blank_line_pos + 4);
  }

  // Parse headers
  std::map<std::string, std::string> response_headers;
  int status_code = 200; // Default status

  if (!headers_section.empty()) {
    std::istringstream header_stream(headers_section);
    std::string line;
    while (std::getline(header_stream, line)) {
      // Remove trailing \r if present
      if (!line.empty() && line[line.length() - 1] == '\r') {
        line = line.substr(0, line.length() - 1);
      }

      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::string header_name = line.substr(0, colon_pos);
        std::string header_value = line.substr(colon_pos + 1);

        // Trim leading whitespace from value
        size_t value_start = header_value.find_first_not_of(" \t");
        if (value_start != std::string::npos) {
          header_value = header_value.substr(value_start);
        }

        // Check for Status header
        if (header_name == "Status") {
          // Parse status code from value (e.g., "404 Not Found")
          std::istringstream status_stream(header_value);
          status_stream >> status_code;
        }

        // Store header as string (HTTP/1.1 standard)
        response_headers[header_name] = header_value;
      }
    }
  }

  // Create Http::Body from body section
  Http::Body::Value body_val;
  body_val.html_raw = new std::string(body_section);
  Http::Body result_body(Http::Body::Html, body_val);

  // Create Http::Response
  Http::Response response(status_code, response_headers, result_body);

  return OK(Http::Response, response);
}

CgiDelegate::~CgiDelegate() {
  // env is now a value member, will be automatically destroyed
}
