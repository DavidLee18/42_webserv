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

ServerName *ServerName::host(std::list<std::string> hostparts) {
  return new ServerName(
      Host,
      (ServerName::Val){.host_name = new std::list<std::string>(hostparts)});
}

ServerName *ServerName::ipv4(unsigned char b1, unsigned char b2,
                             unsigned char b3, unsigned char b4) {
  return new ServerName(Ipv4, (ServerName::Val){.ipv4 = {b1, b2, b3, b4}});
}

Result<std::pair<ServerName *, size_t> >
ServerName::Parser::parse_host(std::string raw) {
  std::stringstream ss(raw);
  std::list<std::string> parts;
  std::string part;
  bool dom_end = false;
  size_t j = 0;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    if ((!dom_end && !std::isalnum(part[0])) || !std::isalpha(part[0]))
      return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
    j++;
    for (size_t i = 1; i < part.size() - 1; i++) {
      if (!std::isalnum(part[i]) && part[i] != '-')
        return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
    }
    if (!std::isalnum(part[part.size() - 1]))
      return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
    j += part.size();
    std::getline(ss, part, '.');
  }
  if (!ss.eof())
    return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  if ((!dom_end && !std::isalnum(part[0])) || !std::isalpha(part[0]))
    return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  j++;
  for (size_t i = 1; i < part.size() - 1; i++) {
    if (!std::isalnum(part[i]) && part[i] != '-')
      return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  }
  if (!std::isalnum(part[part.size() - 1]))
    return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  return OK_PAIR(ServerName *, size_t, ServerName::host(parts),
                 j + part.size());
}

Result<std::pair<ServerName *, size_t> >
ServerName::Parser::parse_ipv4(std::string raw) {
  std::stringstream ss(raw);
  std::vector<std::string> *ips = new std::vector<std::string>();
  std::vector<unsigned char> addrs;
  std::string part;
  size_t i = 0;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    if (part.size() < 1 || part.size() > 3)
      return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
    for (size_t j = 0; j < part.size(); j++) {
      if ('0' < part[j] || part[j] > '9')
        return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
    }
    i += part.size();
    addrs.push_back(static_cast<unsigned char>(std::atoi(part.c_str())));
    ips->push_back(part);
    std::getline(ss, part, '.');
  }
  if (!ss.eof())
    return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  if (part.size() < 1 || part.size() > 3)
    return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  for (size_t j = 0; j < part.size(); j++) {
    if ('0' < part[j] || part[j] > '9')
      return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  }
  addrs.push_back(static_cast<unsigned char>(std::atoi(part.c_str())));
  ips->push_back(part);
  if (addrs.size() != 4)
    return ERR_PAIR(ServerName *, size_t, Errors::invalid_format);
  return OK_PAIR(ServerName *, size_t,
                 ServerName::ipv4(addrs[0], addrs[1], addrs[2], addrs[3]),
                 i + part.size());
}

Result<std::pair<ServerName *, size_t> >
ServerName::Parser::parse(std::string raw) {
  Result<std::pair<ServerName *, size_t> > res = parse_host(raw);
  if (res.error().empty())
    return res;
  return parse_ipv4(raw);
}

CgiMetaVar *CgiMetaVar::auth_type(CgiAuthType ty) {
  return new CgiMetaVar(AUTH_TYPE,
                        (CgiMetaVar::Val){.auth_type = new CgiAuthType(ty)});
}

CgiMetaVar *CgiMetaVar::content_length(unsigned int l) {
  return new CgiMetaVar(CONTENT_LENGTH, (CgiMetaVar::Val){.content_length = l});
}

CgiMetaVar *CgiMetaVar::content_type(ContentType ty) {
  return new CgiMetaVar(CONTENT_TYPE,
                        (CgiMetaVar::Val){.content_type = new ContentType(ty)});
}

CgiMetaVar *CgiMetaVar::gateway_interface(GatewayInterface i) {
  return new CgiMetaVar(GATEWAY_INTERFACE,
                        (CgiMetaVar::Val){.gateway_interface = i});
}

CgiMetaVar *CgiMetaVar::path_info(std::list<std::string> parts) {
  return new CgiMetaVar(
      PATH_INFO,
      (CgiMetaVar::Val){.path_info = new std::list<std::string>(parts)});
}

CgiMetaVar *CgiMetaVar::path_translated(std::string path) {
  return new CgiMetaVar(
      PATH_TRANSLATED,
      (CgiMetaVar::Val){.path_translated = new std::string(path)});
}

CgiMetaVar *
CgiMetaVar::query_string(std::map<std::string, std::string> query_map) {
  return new CgiMetaVar(
      QUERY_STRING,
      (CgiMetaVar::Val){.query_string =
                            new std::map<std::string, std::string>(query_map)});
}

CgiMetaVar *CgiMetaVar::remote_addr(unsigned char a, unsigned char b,
                                    unsigned char c, unsigned char d) {
  return new CgiMetaVar(REMOTE_ADDR,
                        (CgiMetaVar::Val){.remote_addr = {a, b, c, d}});
}

CgiMetaVar *CgiMetaVar::remote_host(std::list<std::string> parts) {
  return new CgiMetaVar(
      REMOTE_HOST,
      (CgiMetaVar::Val){.remote_host = new std::list<std::string>(parts)});
}

CgiMetaVar *CgiMetaVar::remote_ident(std::string id) {
  return new CgiMetaVar(REMOTE_IDENT,
                        (CgiMetaVar::Val){.remote_ident = new std::string(id)});
}

CgiMetaVar *CgiMetaVar::remote_user(std::string user) {
  return new CgiMetaVar(
      REMOTE_USER, (CgiMetaVar::Val){.remote_user = new std::string(user)});
}

CgiMetaVar *CgiMetaVar::request_method(Http::Method method) {
  return new CgiMetaVar(REQUEST_METHOD,
                        (CgiMetaVar::Val){.request_method = method});
}

CgiMetaVar *CgiMetaVar::script_name(std::list<std::string> parts) {
  return new CgiMetaVar(
      SCRIPT_NAME,
      (CgiMetaVar::Val){.script_name = new std::list<std::string>(parts)});
}

CgiMetaVar *CgiMetaVar::server_name(ServerName *srv) {
  return new CgiMetaVar(SERVER_NAME, (CgiMetaVar::Val){.server_name = srv});
}

CgiMetaVar *CgiMetaVar::server_port(unsigned short port) {
  return new CgiMetaVar(SERVER_PORT, (CgiMetaVar::Val){.server_port = port});
}

CgiMetaVar *CgiMetaVar::server_protocol(ServerProtocol proto) {
  return new CgiMetaVar(SERVER_PROTOCOL,
                        (CgiMetaVar::Val){.server_protocol = proto});
}

CgiMetaVar *CgiMetaVar::server_software(ServerSoftware soft) {
  return new CgiMetaVar(SERVER_SOFTWARE,
                        (CgiMetaVar::Val){.server_software = soft});
}

CgiMetaVar *CgiMetaVar::custom_var(EtcMetaVar::Type ty, std::string name,
                                   std::string value) {
  return new CgiMetaVar(
      X_, (CgiMetaVar::Val){.etc_val = new EtcMetaVar(ty, name, value)});
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_auth_type(std::string raw) {
  std::stringstream ss(raw);
  std::string ty;
  std::getline(ss, ty, ' ');
  if (ss.eof() || !ss)
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  std::transform(ty.begin(), ty.end(), ty.begin(), to_upper);
  if (ty == "basic")
    return OK_PAIR(CgiMetaVar *, size_t,
                   CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Basic)), 5);
  if (ty == "digest")
    return OK_PAIR(CgiMetaVar *, size_t,
                   CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Digest)), 6);
  return OK_PAIR(
      CgiMetaVar *, size_t,
      CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::CgiAuthOther, ty)),
      ty.size());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_content_length(std::string raw) {
  char *ptr = NULL;
  const char *str = raw.c_str();
  unsigned long l = std::strtoul(str, &ptr, 10);
  if (ptr == NULL || *ptr != '\0' || l > UINT32_MAX)
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar *, size_t,
                 CgiMetaVar::content_length(static_cast<unsigned int>(l)),
                 ptr - str);
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_content_type(std::string raw) {
  size_t consumed = 0;
  size_t slash_pos = raw.find('/');
  if (slash_pos == std::string::npos)
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  
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
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  
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
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  subtype = subtype.substr(start, end - start + 1);
  
  ContentType ct(type, subtype);
  
  // Parse parameters if present
  while (consumed < raw.length() && raw[consumed] == ';') {
    consumed++; // skip semicolon
    
    // Skip whitespace
    while (consumed < raw.length() && (raw[consumed] == ' ' || raw[consumed] == '\t'))
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
    while (consumed < raw.length() && (raw[consumed] == ' ' || raw[consumed] == '\t'))
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
      if (param_value.length() >= 2 && param_value[0] == '"' && param_value[param_value.length() - 1] == '"')
        param_value = param_value.substr(1, param_value.length() - 2);
    }
    
    ct.params[param_name] = param_value;
  }
  
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::content_type(ct), consumed);
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_gateway_interface(std::string raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "cgi/1.1" || norm == "cgi-1.1")
    return OK_PAIR(CgiMetaVar *, size_t,
                   CgiMetaVar::gateway_interface(Cgi_1_1), raw.length());
  return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_path_info(std::string raw) {
  if (raw.empty() || raw[0] != '/')
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  
  std::list<std::string> parts;
  std::stringstream ss(raw.substr(1)); // skip leading slash
  std::string part;
  
  while (std::getline(ss, part, '/')) {
    parts.push_back(part);
  }
  
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::path_info(parts),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_path_translated(std::string raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::path_translated(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_query_string(std::string raw) {
  std::map<std::string, std::string> query_map;
  
  if (raw.empty()) {
    return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::query_string(query_map),
                   0);
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
  
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::query_string(query_map),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_remote_addr(std::string raw) {
  std::stringstream ss(raw);
  std::vector<unsigned char> octets;
  std::string octet;
  
  while (std::getline(ss, octet, '.')) {
    if (octet.empty() || octet.length() > 3)
      return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
    
    for (size_t i = 0; i < octet.length(); i++) {
      if (octet[i] < '0' || octet[i] > '9')
        return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
    }
    
    long val = std::atol(octet.c_str());
    if (val < 0 || val > 255)
      return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
    
    octets.push_back(static_cast<unsigned char>(val));
  }
  
  if (octets.size() != 4)
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  
  return OK_PAIR(CgiMetaVar *, size_t,
                 CgiMetaVar::remote_addr(octets[0], octets[1], octets[2],
                                        octets[3]),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_remote_host(std::string raw) {
  Result<std::pair<ServerName *, size_t> > server_res =
      ServerName::Parser::parse(raw);
  if (!server_res.error().empty())
    return ERR_PAIR(CgiMetaVar *, size_t, server_res.error());
  
  // ServerName parser returns a ServerName, but we need a list of strings
  // For simplicity, we'll parse it as a hostname
  std::list<std::string> parts;
  std::stringstream ss(raw);
  std::string part;
  
  while (std::getline(ss, part, '.')) {
    parts.push_back(part);
  }
  
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::remote_host(parts),
                 server_res.value()->second);
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_remote_ident(std::string raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::remote_ident(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_remote_user(std::string raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::remote_user(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
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
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::request_method(m),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_script_name(std::string raw) {
  if (raw.empty() || raw[0] != '/')
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  
  std::list<std::string> parts;
  std::stringstream ss(raw.substr(1)); // skip leading slash
  std::string part;
  
  while (std::getline(ss, part, '/')) {
    parts.push_back(part);
  }
  
  return OK_PAIR(CgiMetaVar *, size_t, CgiMetaVar::script_name(parts),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_server_name(std::string raw) {
  Result<std::pair<ServerName *, size_t> > res =
      ServerName::Parser::parse(raw);
  if (!res.error().empty())
    return ERR_PAIR(CgiMetaVar *, size_t, res.error());
  
  return OK_PAIR(CgiMetaVar *, size_t,
                 CgiMetaVar::server_name(res.value()->first),
                 res.value()->second);
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_server_port(std::string raw) {
  char *ptr = NULL;
  const char *str = raw.c_str();
  unsigned long port = std::strtoul(str, &ptr, 10);
  if (ptr == str || *ptr != '\0' || port > 65535)
    return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar *, size_t,
                 CgiMetaVar::server_port(static_cast<unsigned short>(port)),
                 raw.length());
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_server_protocol(std::string raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "http/1.1" || norm == "http-1.1")
    return OK_PAIR(CgiMetaVar *, size_t,
                   CgiMetaVar::server_protocol(Http_1_1), raw.length());
  return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_server_software(std::string raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "webserv")
    return OK_PAIR(CgiMetaVar *, size_t,
                   CgiMetaVar::server_software(Webserv), raw.length());
  return ERR_PAIR(CgiMetaVar *, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar *, size_t> >
CgiMetaVar::Parser::parse_custom_var(std::string name, std::string value) {
  EtcMetaVar::Type type = EtcMetaVar::Custom;
  if (name.length() >= 5 && name.substr(0, 5) == "HTTP_") {
    type = EtcMetaVar::Http;
  }
  return OK_PAIR(CgiMetaVar *, size_t,
                 CgiMetaVar::custom_var(type, name, value),
                 name.length() + value.length());
}

unsigned char to_upper(unsigned char c) {
  return static_cast<unsigned char>(std::toupper(static_cast<int>(c)));
}
