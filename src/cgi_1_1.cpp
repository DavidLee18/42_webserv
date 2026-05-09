#include "webserv.h"
#include <cstdio>
#include <fcntl.h>

CgiAuthType::CgiAuthType(const CgiAuthType::Type type)
    : _type(type), _other(NULL) {}

CgiAuthType::CgiAuthType(const CgiAuthType::Type type, const std::string &other)
    : _type(type), _other(new std::string(other)) {}

CgiAuthType::CgiAuthType(const CgiAuthType &other) : _type(other._type) {
  if (other._other != NULL) {
    _other = new std::string(*other._other);
  } else {
    _other = NULL;
  }
}

CgiAuthType &CgiAuthType::operator=(const CgiAuthType &other) {
  if (this != &other) {
    delete _other;
    _type = other._type;
    if (other._other != NULL) {
      _other = new std::string(*other._other);
    } else {
      _other = NULL;
    }
  }
  return *this;
}

CgiAuthType::~CgiAuthType() { delete _other; }

CgiAuthType::Type const &CgiAuthType::type() const { return _type; }

std::string const *CgiAuthType::other() const { return _other; }

ContentType::ContentType(const ContentType::Type ty, std::string const &subty)
    : type(ty), subtype(subty), params() {}

ContentType::ContentType(ContentType const &other)
    : type(other.type), subtype(other.subtype), params(other.params) {}

ContentType &ContentType::operator=(const ContentType &other) {
  if (this != &other) {
    type = other.type;
    subtype = other.subtype;
    params = other.params;
  }
  return *this;
}

Result<Void> ContentType::add_param(const std::string &k,
                                    const std::string &v) {
  const std::map<std::string, std::string>::iterator iter = params.find(k);
  if (iter == params.end())
    return ERR(Void, Errors::not_found);
  iter->second = v;
  return OKV;
}

ServerName ServerName::host(const std::list<std::string> &hostparts) {
  return ServerName(
      Host,
      (ServerName::Val){.host_name = new std::list<std::string>(hostparts)});
}

ServerName ServerName::ipv4(const unsigned char b1, const unsigned char b2,
                            const unsigned char b3, const unsigned char b4) {
  return ServerName(Ipv4, (ServerName::Val){.ipv4 = {b1, b2, b3, b4}});
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse_host(const std::string &raw) {
  std::stringstream ss(raw);
  const std::list<std::string> parts;
  std::string part;
  const bool dom_end = false;
  size_t j = 0;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    if (part.empty())
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    if (!std::isalnum(static_cast<unsigned char>(part[0])) ||
        !std::isalpha(static_cast<unsigned char>(part[0])))
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    j++;
    if (part.size() > 1) {
      for (size_t i = 1; i < part.size() - 1; i++) {
        if (!std::isalnum(static_cast<unsigned char>(part[i])) &&
            part[i] != '-')
          return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
      }
      if (!std::isalnum(static_cast<unsigned char>(part[part.size() - 1])))
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    }
    j += part.size();
    std::getline(ss, part, '.');
  }
  if (!ss.eof())
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  if (part.empty())
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  if ((!dom_end && !std::isalnum(static_cast<unsigned char>(part[0]))) ||
      !std::isalpha(static_cast<unsigned char>(part[0])))
    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  j++;
  if (part.size() > 1) {
    for (size_t i = 1; i < part.size() - 1; i++) {
      if (!std::isalnum(static_cast<unsigned char>(part[i])) && part[i] != '-')
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    }
    if (!std::isalnum(static_cast<unsigned char>(part[part.size() - 1])))
      return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
  }
  return OK_PAIR(ServerName, size_t, ServerName::host(parts), j + part.size());
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse_ipv4(const std::string &raw) {
  std::stringstream ss(raw);
  std::vector<unsigned char> addrs;
  std::string part;
  size_t i = 0;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    if (part.empty() || part.size() > 3)
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
  if (part.empty() || part.size() > 3)
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
ServerName::Parser::parse(const std::string &raw) {
  Result<std::pair<ServerName, size_t> > res = parse_host(raw);
  if (res.error().empty())
    return res;
  return parse_ipv4(raw);
}

ServerName::ServerName(const ServerName::Type ty, const ServerName::Val v)
    : type(ty), val(v) {}

ServerName::ServerName(const ServerName &other) : type(other.type), val() {
  if (type == Host) {
    val.host_name = new std::list<std::string>(*other.val.host_name);
  } else {
    val.ipv4[0] = other.val.ipv4[0];
    val.ipv4[1] = other.val.ipv4[1];
    val.ipv4[2] = other.val.ipv4[2];
    val.ipv4[3] = other.val.ipv4[3];
  }
}

ServerName::~ServerName() {
  if (type == ServerName::Host) {
    delete val.host_name;
  }
}

ServerName &ServerName::operator=(const ServerName &other) {
  if (this != &other) {
    if (type == Host) {
      delete val.host_name;
    }

    type = other.type;
    if (type == Host) {
      val.host_name = new std::list<std::string>(*other.val.host_name);
    } else {
      val.ipv4[0] = other.val.ipv4[0];
      val.ipv4[1] = other.val.ipv4[1];
      val.ipv4[2] = other.val.ipv4[2];
      val.ipv4[3] = other.val.ipv4[3];
    }
  }
  return *this;
}

ServerName::Type const &ServerName::get_type() const { return type; }
ServerName::Val const &ServerName::get_val() const { return val; }

std::ostream &operator<<(std::ostream &os, ServerName const &srvn) {
  if (srvn.get_type() == ServerName::Host) {
    bool first = true;
    for (std::list<std::string>::const_iterator it =
             srvn.get_val().host_name->begin();
         it != srvn.get_val().host_name->end(); ++it) {
      if (!first)
        os << '.';
      os << *it;
      first = false;
    }
    return os;
  } else {
    os << static_cast<int>(srvn.get_val().ipv4[0]) << "."
       << static_cast<int>(srvn.get_val().ipv4[1]) << "."
       << static_cast<int>(srvn.get_val().ipv4[2]) << "."
       << static_cast<int>(srvn.get_val().ipv4[3]);
    return os;
  }
}

EtcMetaVar::EtcMetaVar(const EtcMetaVar::Type ty, std::string const &n,
                       std::string const &v)
    : type(ty), name(n), value(v) {}

EtcMetaVar::EtcMetaVar(EtcMetaVar const &other)
    : type(other.type), name(other.name), value(other.value) {}

EtcMetaVar &EtcMetaVar::operator=(const EtcMetaVar &other) {
  if (this != &other) {
    type = other.type;
    name = other.name;
    value = other.value;
  }
  return *this;
}

EtcMetaVar::Type const &EtcMetaVar::get_type() const { return type; }
std::string const &EtcMetaVar::get_name() const { return name; }
std::string const &EtcMetaVar::get_value() const { return value; }

CgiMetaVar::CgiMetaVar(const CgiMetaVar::Name n, const CgiMetaVar::Val v)
    : name(n), val(v) {}

CgiMetaVar::Name const &CgiMetaVar::get_name() const { return name; }
CgiMetaVar::Val const &CgiMetaVar::get_val() const { return val; }

CgiMetaVar::CgiMetaVar(const CgiMetaVar &other) : name(other.name), val() {
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
    val.query_string =
        new std::map<std::string, std::string>(*other.val.query_string);
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
  case REQUEST_METHOD:
    val.request_method = other.val.request_method;
    break;
  case SCRIPT_NAME:
    val.script_name = new std::list<std::string>(*other.val.script_name);
    break;
  case SERVER_NAME:
    val.server_name = new ServerName(*other.val.server_name);
    break;
  case SERVER_PORT:
    val.server_port = other.val.server_port;
    break;
  case SERVER_PROTOCOL:
    val.server_protocol = other.val.server_protocol;
    break;
  case SERVER_SOFTWARE:
    val.server_software = other.val.server_software;
    break;
  case X_:
    val.etc_val = new EtcMetaVar(*other.val.etc_val);
    break;
  }
}

CgiMetaVar &CgiMetaVar::operator=(const CgiMetaVar &other) {
  if (this != &other) {
    switch (name) {
    case AUTH_TYPE:
      delete val.auth_type;
      break;
    case CONTENT_TYPE:
      delete val.content_type;
      break;
    case PATH_INFO:
      delete val.path_info;
      break;
    case PATH_TRANSLATED:
      delete val.path_translated;
      break;
    case QUERY_STRING:
      delete val.query_string;
      break;
    case REMOTE_HOST:
      delete val.remote_host;
      break;
    case REMOTE_IDENT:
      delete val.remote_ident;
      break;
    case REMOTE_USER:
      delete val.remote_user;
      break;
    case SCRIPT_NAME:
      delete val.script_name;
      break;
    case SERVER_NAME:
      delete val.server_name;
      break;
    case X_:
      delete val.etc_val;
      break;
    default:
      break;
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
      val.query_string =
          new std::map<std::string, std::string>(*other.val.query_string);
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
    case REQUEST_METHOD:
      val.request_method = other.val.request_method;
      break;
    case SCRIPT_NAME:
      val.script_name = new std::list<std::string>(*other.val.script_name);
      break;
    case SERVER_NAME:
      val.server_name = new ServerName(*other.val.server_name);
      break;
    case SERVER_PORT:
      val.server_port = other.val.server_port;
      break;
    case SERVER_PROTOCOL:
      val.server_protocol = other.val.server_protocol;
      break;
    case SERVER_SOFTWARE:
      val.server_software = other.val.server_software;
      break;
    case X_:
      val.etc_val = new EtcMetaVar(*other.val.etc_val);
      break;
    }
  }
  return *this;
}

CgiMetaVar::~CgiMetaVar() {
  switch (name) {
  case AUTH_TYPE:
    delete val.auth_type;
    break;
  case CONTENT_TYPE:
    delete val.content_type;
    break;
  case PATH_INFO:
    delete val.path_info;
    break;
  case PATH_TRANSLATED:
    delete val.path_translated;
    break;
  case QUERY_STRING:
    delete val.query_string;
    break;
  case REMOTE_HOST:
    delete val.remote_host;
    break;
  case REMOTE_IDENT:
    delete val.remote_ident;
    break;
  case REMOTE_USER:
    delete val.remote_user;
    break;
  case SCRIPT_NAME:
    delete val.script_name;
    break;
  case SERVER_NAME:
    delete val.server_name;
    break;
  case X_:
    delete val.etc_val;
    break;
  default:
    break;
  }
}

CgiMetaVar CgiMetaVar::auth_type(const CgiAuthType &ty) {
  return CgiMetaVar(AUTH_TYPE,
                    (CgiMetaVar::Val){.auth_type = new CgiAuthType(ty)});
}

CgiMetaVar CgiMetaVar::content_length(const unsigned int l) {
  return CgiMetaVar(CONTENT_LENGTH, (CgiMetaVar::Val){.content_length = l});
}

CgiMetaVar CgiMetaVar::content_type(const ContentType &ty) {
  return CgiMetaVar(CONTENT_TYPE,
                    (CgiMetaVar::Val){.content_type = new ContentType(ty)});
}

CgiMetaVar CgiMetaVar::gateway_interface(const GatewayInterface i) {
  return CgiMetaVar(GATEWAY_INTERFACE,
                    (CgiMetaVar::Val){.gateway_interface = i});
}

CgiMetaVar CgiMetaVar::path_info(const std::list<std::string> &parts) {
  return CgiMetaVar(
      PATH_INFO,
      (CgiMetaVar::Val){.path_info = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::path_translated(const std::string &path) {
  return CgiMetaVar(
      PATH_TRANSLATED,
      (CgiMetaVar::Val){.path_translated = new std::string(path)});
}

CgiMetaVar
CgiMetaVar::query_string(const std::map<std::string, std::string> &query_map) {
  return CgiMetaVar(
      QUERY_STRING,
      (CgiMetaVar::Val){.query_string =
                            new std::map<std::string, std::string>(query_map)});
}

CgiMetaVar CgiMetaVar::remote_addr(const unsigned char a, const unsigned char b,
                                   const unsigned char c,
                                   const unsigned char d) {
  return CgiMetaVar(REMOTE_ADDR,
                    (CgiMetaVar::Val){.remote_addr = {a, b, c, d}});
}

CgiMetaVar CgiMetaVar::remote_host(const std::list<std::string> &parts) {
  return CgiMetaVar(
      REMOTE_HOST,
      (CgiMetaVar::Val){.remote_host = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::remote_ident(const std::string &id) {
  return CgiMetaVar(REMOTE_IDENT,
                    (CgiMetaVar::Val){.remote_ident = new std::string(id)});
}

CgiMetaVar CgiMetaVar::remote_user(const std::string &user) {
  return CgiMetaVar(REMOTE_USER,
                    (CgiMetaVar::Val){.remote_user = new std::string(user)});
}

CgiMetaVar CgiMetaVar::request_method(const Request::Method method) {
  return CgiMetaVar(REQUEST_METHOD,
                    (CgiMetaVar::Val){.request_method = method});
}

CgiMetaVar CgiMetaVar::script_name(const std::list<std::string> &parts) {
  return CgiMetaVar(
      SCRIPT_NAME,
      (CgiMetaVar::Val){.script_name = new std::list<std::string>(parts)});
}

CgiMetaVar CgiMetaVar::server_name(const ServerName &srv) {
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
                                  const std::string &name,
                                  const std::string &value) {
  return CgiMetaVar(
      X_, (CgiMetaVar::Val){.etc_val = new EtcMetaVar(ty, name, value)});
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_auth_type(const std::string &raw) {
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
CgiMetaVar::Parser::parse_content_length(const std::string &raw) {
  char *ptr = NULL;
  const char *str = raw.c_str();
  const unsigned long l = std::strtoul(str, &ptr, 10);
  if (ptr == NULL || *ptr != '\0' || l > UINT32_MAX)
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t,
                 CgiMetaVar::content_length(static_cast<unsigned int>(l)),
                 ptr - str);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_content_type(std::string raw) {
  size_t consumed = 0;
  const size_t slash_pos = raw.find('/');
  if (slash_pos == std::string::npos)
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);

  std::string type_str = raw.substr(0, slash_pos);
  std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::tolower);

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

  const size_t semicolon_pos = raw.find(';', consumed);
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
    const size_t eq_pos = raw.find('=', consumed);
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
    const size_t next_semi = raw.find(';', consumed);
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
CgiMetaVar::Parser::parse_gateway_interface(const std::string &raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "cgi/1.1" || norm == "cgi-1.1")
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::gateway_interface(Cgi_1_1),
                   raw.length());
  return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_path_info(const std::string &raw) {
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
CgiMetaVar::Parser::parse_path_translated(const std::string &raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::path_translated(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_query_string(const std::string &raw) {
  std::map<std::string, std::string> query_map;

  if (raw.empty()) {
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::query_string(query_map), 0);
  }

  std::stringstream ss(raw);
  std::string pair;

  while (std::getline(ss, pair, '&')) {
    const size_t eq_pos = pair.find('=');
    if (eq_pos == std::string::npos) {
      query_map[pair] = "";
    } else {
      std::string key = pair.substr(0, eq_pos);
      const std::string value = pair.substr(eq_pos + 1);
      query_map[key] = value;
    }
  }

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::query_string(query_map),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_addr(const std::string &raw) {
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

    const long val = std::atol(octet.c_str());
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
CgiMetaVar::Parser::parse_remote_host(const std::string &raw) {
  const Result<std::pair<ServerName, size_t> > server_res =
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
CgiMetaVar::Parser::parse_remote_ident(const std::string &raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_ident(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_remote_user(const std::string &raw) {
  if (raw.empty())
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::remote_user(raw),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_request_method(const std::string &raw) {
  std::string method = raw;
  std::transform(method.begin(), method.end(), method.begin(), to_upper);

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
CgiMetaVar::Parser::parse_script_name(const std::string &raw) {
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
CgiMetaVar::Parser::parse_server_name(const std::string &raw) {
  const Result<std::pair<ServerName, size_t> > res =
      ServerName::Parser::parse(raw);
  if (!res.error().empty())
    return ERR_PAIR(CgiMetaVar, size_t, res.error());

  return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::server_name(res.value().first),
                 res.value().second);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_port(const std::string &raw) {
  char *ptr = NULL;
  const char *str = raw.c_str();
  const unsigned long port = std::strtoul(str, &ptr, 10);
  if (ptr == str || *ptr != '\0' || port > 65535)
    return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
  return OK_PAIR(CgiMetaVar, size_t,
                 CgiMetaVar::server_port(static_cast<unsigned short>(port)),
                 raw.length());
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_protocol(const std::string &raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "http/1.1" || norm == "http-1.1")
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::server_protocol(Http_1_1),
                   raw.length());
  return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_server_software(const std::string &raw) {
  std::string norm = raw;
  std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
  if (norm == "webserv")
    return OK_PAIR(CgiMetaVar, size_t, CgiMetaVar::server_software(Webserv),
                   raw.length());
  return ERR_PAIR(CgiMetaVar, size_t, Errors::invalid_format);
}

Result<std::pair<CgiMetaVar, size_t> >
CgiMetaVar::Parser::parse_custom_var(const std::string &name,
                                     const std::string &value) {
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
CgiInput::CgiInput() : mvars(), req_body() {}

CgiInput::CgiInput(std::vector<CgiMetaVar> const &vars, const std::string &body)
    : mvars(vars), req_body(body) {}

CgiInput::CgiInput(Request const &req) : mvars(), req_body(req.get_body()) {}

CgiInput::CgiInput(const CgiInput &other)
    : mvars(other.mvars), req_body(other.req_body) {}

CgiInput &CgiInput::operator=(const CgiInput &other) {
  if (this != &other) {
    mvars = other.mvars;
    req_body = other.req_body;
  }
  return *this;
}

Result<CgiInput> CgiInput::Parser::parse(Request const &req) {
  CgiInput input;
  input.req_body = req.get_body();

  // Convert Request::Method to Request::Method for backwards compatibility
  Request::Method h_method;
  switch (req.get_method()) {
  case Request::GET:
    h_method = Request::GET;
    break;
  case Request::HEAD:
    h_method = Request::HEAD;
    break;
  case Request::POST:
    h_method = Request::POST;
    break;
  case Request::PUT:
    h_method = Request::PUT;
    break;
  case Request::DELETE:
    h_method = Request::DELETE;
    break;
  case Request::OPTIONS:
    h_method = Request::OPTIONS;
    break;
  case Request::CONNECT:
    h_method = Request::CONNECT;
    break;
  case Request::TRACE:
    h_method = Request::TRACE;
    break;
  case Request::PATCH:
    h_method = Request::PATCH;
    break;
  default:
    h_method = Request::GET;
    break;
  }

  input.mvars.push_back(CgiMetaVar::request_method(h_method));
  input.mvars.push_back(CgiMetaVar::server_protocol(Http_1_1));
  input.mvars.push_back(CgiMetaVar::gateway_interface(Cgi_1_1));
  input.mvars.push_back(CgiMetaVar::server_software(Webserv));

  // Parse path for SCRIPT_NAME, PATH_INFO, and QUERY_STRING
  std::string path = req.get_path();
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

  // Build path segments list (shared by SCRIPT_NAME and PATH_INFO)
  std::list<std::string> path_parts;
  if (!script_path.empty()) {
    std::string p =
        (script_path[0] == '/') ? script_path.substr(1) : script_path;
    std::stringstream ss(p);
    std::string part;
    while (std::getline(ss, part, '/')) {
      path_parts.push_back(part);
    }
  }

  // Add SCRIPT_NAME
  input.mvars.push_back(CgiMetaVar::script_name(path_parts));

  // Add PATH_INFO. RFC 3875 §4.1.5 defines PATH_INFO as the extra path
  // information after the script name in the URI. Because this server does not
  // split the request URI into a script-name portion and an extra-path portion,
  // PATH_INFO is set to the same segments as SCRIPT_NAME (i.e., the full
  // request path). Scripts that rely on PATH_INFO being distinct from
  // SCRIPT_NAME will need the caller to pre-populate it via add_mvar().
  input.mvars.push_back(CgiMetaVar::path_info(path_parts));

  // Add PATH_TRANSLATED (empty — document root is not available here)
  input.mvars.push_back(CgiMetaVar::path_translated(""));

  // Add QUERY_STRING
  if (!query_string.empty()) {
    std::map<std::string, std::string> query_map;
    std::stringstream ss(query_string);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
      size_t eq_pos = pair.find('=');
      if (eq_pos != std::string::npos)
        query_map[pair.substr(0, eq_pos)] = pair.substr(eq_pos + 1);
    }
    input.mvars.push_back(CgiMetaVar::query_string(query_map));
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

  // Pre-scan headers for Host and Authorization, used to populate SERVER_NAME,
  // SERVER_PORT, AUTH_TYPE, and REMOTE_USER before processing all headers.
  std::map<std::string, std::string> const &headers = req.get_headers();
  std::string host_header_val;
  std::string auth_header_val;
  for (std::map<std::string, std::string>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    std::string hname = it->first;
    for (size_t i = 0; i < hname.size(); i++)
      hname[i] =
          static_cast<char>(to_upper(static_cast<unsigned char>(hname[i])));
    if (hname == "HOST")
      host_header_val = it->second;
    else if (hname == "AUTHORIZATION")
      auth_header_val = it->second;
  }

  // Add SERVER_NAME and SERVER_PORT (from Host header; default localhost:80)
  {
    std::string server_name_str = "localhost";
    unsigned short server_port_val = 80;
    if (!host_header_val.empty()) {
      size_t colon_pos = host_header_val.find(':');
      if (colon_pos != std::string::npos) {
        server_name_str = host_header_val.substr(0, colon_pos);
        std::string port_str = host_header_val.substr(colon_pos + 1);
        char *endptr = NULL;
        unsigned long port = std::strtoul(port_str.c_str(), &endptr, 10);
        if (endptr != NULL && endptr != port_str.c_str() && *endptr == '\0' &&
            port > 0 && port <= 65535)
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
    size_t sp = auth_header_val.find(' ');
    std::string scheme = (sp != std::string::npos)
                             ? auth_header_val.substr(0, sp)
                             : auth_header_val;
    std::string scheme_upper = scheme;
    for (size_t i = 0; i < scheme_upper.size(); i++)
      scheme_upper[i] = static_cast<char>(
          to_upper(static_cast<unsigned char>(scheme_upper[i])));
    if (scheme_upper == "BASIC")
      input.mvars.push_back(
          CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Basic)));
    else if (scheme_upper == "DIGEST")
      input.mvars.push_back(
          CgiMetaVar::auth_type(CgiAuthType(CgiAuthType::Digest)));
    else
      input.mvars.push_back(CgiMetaVar::auth_type(
          CgiAuthType(CgiAuthType::CgiAuthOther, scheme)));
    // REMOTE_USER: username is not decoded here (would require Base64 decode
    // for Basic); set to empty string
    input.mvars.push_back(CgiMetaVar::remote_user(""));
  }

  // Add HTTP headers as CGI variables
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

  return OK(CgiInput, input);
}

Result<Void> CgiInput::add_mvar(std::string const &name,
                                std::string const &val) {
  const Result<std::pair<CgiMetaVar, size_t> > res =
      CgiMetaVar::Parser::parse(name, val);
  if (!res.has_value())
    return ERR(Void, "CgiMetaVar parse failed");
  mvars.push_back(res.value().first);
  return OKV;
}

char **CgiInput::to_envp() const {
  char **envp = new char *[mvars.size() + 1];

  for (size_t i = 0; i < mvars.size(); i++) {
    CgiMetaVar const &var = mvars[i];
    std::string env_str;

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
      case Request::GET:
        env_str += "GET";
        break;
      case Request::HEAD:
        env_str += "HEAD";
        break;
      case Request::POST:
        env_str += "POST";
        break;
      case Request::PUT:
        env_str += "PUT";
        break;
      case Request::DELETE:
        env_str += "DELETE";
        break;
      case Request::OPTIONS:
        env_str += "OPTIONS";
        break;
      case Request::CONNECT:
        env_str += "CONNECT";
        break;
      case Request::TRACE:
        env_str += "TRACE";
        break;
      case Request::PATCH:
        env_str += "PATCH";
        break;
      default:
        env_str += "UNKNOWN";
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

unsigned char to_upper(const unsigned char c) {
  return static_cast<unsigned char>(std::toupper(static_cast<int>(c)));
}

CgiDelegate::CgiDelegate(Request const &req, EPoll &ep)
    : _env(), _script_path(), _req(req), _epoll(ep), _pid(-1), _stdin(NULL),
      _stdout(NULL), _total_written(0), _output(), _state(NotRegistered),
      _start_time(), _timeout_ns(0) {}

Result<CgiDelegate> CgiDelegate::from_req(const Request &req, EPoll &ep,
                                          const RouteRule_CGI &rule) {
  CgiDelegate del(req, ep);
  TRY(CgiDelegate, CgiInput, del._env, CgiInput::Parser::parse(req))
  char pwd[PATH_MAX];
  if (getcwd(pwd, sizeof(pwd)) == NULL)
    return ERR(CgiDelegate, "getting PWD failed");
  del._script_path = pwd + std::string("/");
  del._script_path += rule.get_executable();
  if (rule.get_timeout() <= 0)
    return ERR(CgiDelegate, "timeout must be positive");
  del._timeout_ns = static_cast<size_t>(rule.get_timeout() * 1e9);
  std::map<std::string, std::string> vars(rule.get_env());
  for (std::map<std::string, std::string>::const_iterator it = vars.begin();
       it != vars.end(); ++it) {
    del._env.add_mvar(it->first, it->second);
  }
  return OK(CgiDelegate, del);
}

CgiDelegate::CgiDelegate(const CgiDelegate &other)
    : _env(other._env), _script_path(other._script_path), _req(other._req),
      _epoll(other._epoll), _pid(other._pid), _stdin(other._stdin),
      _stdout(other._stdout), _total_written(other._total_written),
      _output(other._output), _state(other._state), _start_time(),
      _timeout_ns() {
  const_cast<CgiDelegate &>(other)._env.mvars.clear();
  const_cast<CgiDelegate &>(other)._env.req_body.clear();
  const_cast<CgiDelegate &>(other)._script_path.clear();
  const_cast<CgiDelegate &>(other)._pid = -1;
  const_cast<CgiDelegate &>(other)._stdin = NULL;
  const_cast<CgiDelegate &>(other)._stdout = NULL;
  const_cast<CgiDelegate &>(other)._total_written = 0;
  const_cast<CgiDelegate &>(other)._output.clear();
  const_cast<CgiDelegate &>(other)._state = NotRegistered;
}

static const int kWaitpidPollIntervalUs = 1000;
static const int kMaxReapWaitMs = 50;
static const int kWaitpidReapAttempts =
    (kMaxReapWaitMs * 1000) / kWaitpidPollIntervalUs;

// Reap child without risking an unbounded blocking wait.
// Writes the exit status through 'status' when a child is successfully
// reaped, leaves it untouched otherwise.
static bool waitpid_nohang(const pid_t pid, int *status) {
  int dummy;
  int *s = (status != NULL) ? status : &dummy;
  for (int i = 0; i < kWaitpidReapAttempts; ++i) {
    const pid_t wr = waitpid(pid, s, WNOHANG);
    if (wr == pid) {
      return true;
    }
    if (wr == -1)
      return false;
    usleep(kWaitpidPollIntervalUs);
  }
  return false;
}

static void terminate_child(const pid_t pid) {
  kill(pid, SIGKILL);
  (void)waitpid_nohang(pid, NULL);
}

CgiDelegate &
CgiDelegate::operator=(const CgiDelegate &other) throw(std::logic_error) {
  (void)other;
  throw std::logic_error("CgiDelegate assignment not allowed");
}

// Phase 1: create pipes, fork the CGI process, and register the parent's
// pipe ends with the shared epoll instance. No epoll_wait() is performed
// here - the caller's main loop is the sole owner of epoll_wait() and
// will drive handle_event() for each event delivered.
Result<Void> CgiDelegate::register_(
    std::map<FileDescriptor const *,
             std::pair<FileDescriptor const *, CgiDelegate *> > &cgis,
    FileDescriptor const *client_fd) {
  if (_pid != -1 || _state != NotRegistered)
    return ERR(Void, Errors::cgi_invalid_state);

  Result<std::pair<FileDescriptor, FileDescriptor> > stdin_pipe_res =
      FileDescriptor::pipe();
  Result<std::pair<FileDescriptor, FileDescriptor> > stdout_pipe_res =
      FileDescriptor::pipe();

  if (!stdin_pipe_res.has_value())
    return ERR(Void, "Failed to create stdin pipe");
  if (!stdout_pipe_res.has_value()) {
    {
      FileDescriptor stdin0(stdin_pipe_res.value().first);
      FileDescriptor stdin1(stdin_pipe_res.value().second);
    }
    return ERR(Void, "Failed to create stdout pipe");
  }
  if (!const_cast<FileDescriptor &>(stdin_pipe_res.value().first)
           .close_on_exec()
           .has_value()) {
    {
      FileDescriptor stdin0(stdin_pipe_res.value().first);
      FileDescriptor stdin1(stdin_pipe_res.value().second);
      FileDescriptor stdout0(stdout_pipe_res.value().first);
      FileDescriptor stdout1(stdout_pipe_res.value().second);
    }
    return ERR(Void, "Failed to set stdin pipe to close-on-exec mode");
  }
  if (!const_cast<FileDescriptor &>(stdout_pipe_res.value().second)
           .close_on_exec()
           .has_value()) {
    {
      FileDescriptor stdin0(stdin_pipe_res.value().first);
      FileDescriptor stdin1(stdin_pipe_res.value().second);
      FileDescriptor stdout0(stdout_pipe_res.value().first);
      FileDescriptor stdout1(stdout_pipe_res.value().second);
    }
    return ERR(Void, "Failed to set stdout pipe to close-on-exec mode");
  }

  pid_t pid = fork();
  if (pid == -1) {
    {
      FileDescriptor stdin0(stdin_pipe_res.value().first);
      FileDescriptor stdin1(stdin_pipe_res.value().second);
      FileDescriptor stdout0(stdout_pipe_res.value().first);
      FileDescriptor stdout1(stdout_pipe_res.value().second);
    }
    return ERR(Void, "Failed to fork process");
  } else if (pid == 0) {
    {
      FileDescriptor stdin1 = stdin_pipe_res.value().second;
      FileDescriptor stdout0 = stdout_pipe_res.value().first;
    }

    FileDescriptor &stdin =
        const_cast<FileDescriptor &>(stdin_pipe_res.value().first);
    FileDescriptor &stdout =
        const_cast<FileDescriptor &>(stdout_pipe_res.value().second);

    Result<Void> res = stdin.dup2stdin();
    if (!res.has_value()) {
      std::cerr << res.error() << std::endl;
      std::exit(1);
    }

    res = stdout.dup2stdout();
    if (!res.has_value()) {
      std::cerr << res.error() << std::endl;
      std::exit(1);
    }

    char **envp = _env.to_envp();
    char *argv[2] = {const_cast<char *>(_script_path.c_str()), NULL};

    size_t last_slash = _script_path.rfind('/');
    std::string path;
    if (last_slash == std::string::npos)
      path =
          getenv("PWD") ? getenv("PWD") + std::string("/") + _script_path : "/";
    else
      path = _script_path.substr(0, last_slash + 1);

    argv[0] = const_cast<char *>(path.c_str());
    if (chdir(path.c_str()) != 0) {
      std::cerr << "Failed to change directory to: " << path << std::endl;
      std::exit(1);
    }

    execve(_script_path.c_str(), argv, envp);

    // execve failed
    for (size_t i = 0; envp[i] != NULL; i++)
      delete[] envp[i];
    delete[] envp;

    std::cerr << "Failed to execute CGI script: " << _script_path << std::endl;
    exit(1);
  }

  {
    FileDescriptor _stdin0(stdin_pipe_res.value().first);
    FileDescriptor _stdout1(stdout_pipe_res.value().second);
  }

  _pid = pid;
  FileDescriptor stdin = stdin_pipe_res.value().second;
  FileDescriptor stdout = stdout_pipe_res.value().first;

  // Non-blocking is mandatory: epoll readiness does not imply non-blocking
  // semantics of read/write, and partial IO is expected in the event loop.
  Result<Void> res = stdin.set_nonblocking();
  if (!res.has_value()) {
    _state = Failed;
    return ERR(Void, "Failed to set stdin pipe to non-blocking mode");
  }

  res = stdout.set_nonblocking();
  if (!res.has_value()) {
    _state = Failed;
    return ERR(Void, "Failed to set stdout pipe to non-blocking mode");
  }

  // Register stdin for EPOLLOUT only when we actually have a body to send.
  // If there is no body, let the stdin_fd destructor close the pipe so the
  // CGI script sees EOF on its stdin.
  if (!_req.get_body().empty()) {
    Event write_event(NULL, false, true, false, false, true, true);
    Option write_option(false, false, false, false);
    Result<FileDescriptor *> add_res =
        _epoll.add_fd(stdin, write_event, write_option);
    if (!add_res.has_value()) {
      _state = Failed;
      return ERR(Void, "Failed to add stdin to epoll");
    }
    _stdin = add_res.value();
  } else {
    {
      FileDescriptor stdin_drop(stdin);
    }
    _stdin = NULL;
  }

  // Register stdout for EPOLLIN (plus err/hup so we notice child exit).
  Event read_event(NULL, true, false, true, false, true, true);
  Option read_option(false, false, false, false);
  Result<FileDescriptor *> add_out_res =
      _epoll.add_fd(stdout, read_event, read_option);
  if (!add_out_res.has_value()) {
    if (_stdin != NULL) {
      dprintf(2, "closing stdin: fd=%d\n", _stdin->_fd);
      const int stdin = _stdin->_fd;
      _epoll.del_fd(_stdin);
      const int r = fcntl(stdin, F_GETFD);
      dprintf(2, "F_GETFD stdin(%d): %d (%s)\n", stdin, r, strerror(errno));
      _stdin = NULL;
    }
    _state = Failed;
    return ERR(Void, "Failed to add stdout to epoll");
  }
  _stdout = add_out_res.value();
  if (clock_gettime(CLOCK_MONOTONIC, &_start_time) != 0) {
    _state = Failed;
    return ERR(Void, "Failed to get start time for CGI process");
  }
  cgis[_stdin] = std::make_pair(client_fd, this);
  cgis[_stdout] = std::make_pair(client_fd, this);
  _state = Waiting;
  return OKV;
}

// Phase 2: consume one event delivered by the caller's shared epoll_wait.
// Caller is expected to filter events and only forward those belonging to
// fds this delegate registered. Unknown events are ignored.
Result<Void> CgiDelegate::handle_event(const Event *ev) {
  dprintf(2, "handle_event called: ev_fd=%d stdin=%d stdout=%d\n",
          ev ? ev->fd->_fd : -1, _stdin ? _stdin->_fd : -1,
          _stdout ? _stdout->_fd : -1);
  if (ev == NULL)
    return OKV;
  if (_state == Failed)
    return ERR(Void, Errors::bad_gateway);
  if (_state != Waiting)
    return ERR(Void, Errors::invalid_operation);
  timespec now = {};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    _state = Failed;
    return ERR(Void, Errors::bad_gateway);
  }
  if (static_cast<size_t>(now.tv_sec * 1000000000 + now.tv_nsec) >=
      _timeout_ns + static_cast<size_t>(_start_time.tv_sec * 1000000000 +
                                        _start_time.tv_nsec)) {
    _state = Failed;
    return ERR(Void, Errors::gateway_timeout);
  }
  if (_stdin != NULL && ev->fd == _stdin) {
    if (ev->err) {
      _state = Failed;
      return ERR(Void, Errors::bad_gateway);
    }
    if (ev->hup || ev->rdhup) {
      if (_total_written < _req.get_body().length()) {
        _state = Failed;
        return ERR(Void, Errors::bad_gateway);
      }
      // All data was already written; close stdin and continue.

      dprintf(2, "closing stdin: fd=%d\n", _stdin->_fd);
      const int stdin = _stdin->_fd;
      _epoll.del_fd(_stdin);
      const int r = fcntl(stdin, F_GETFD);
      dprintf(2, "F_GETFD stdin(%d): %d (%s)\n", stdin, r, strerror(errno));
      _stdin = NULL;
      return OKV;
    }
    if (ev->out) {
      if (_total_written < _req.get_body().length()) {
        const Result<ssize_t> written =
            _stdin->pipe_write(_req.get_body().c_str() + _total_written,
                               _req.get_body().length() - _total_written);
        if (written.has_value() && written.value() > 0)
          _total_written += static_cast<size_t>(written.value());
        else if (written.has_value() && written.value() == 0) {
          terminate_child(_pid);
          _pid = -1;
          _state = Failed;
          return ERR(Void, Errors::bad_gateway);
        } else {
          _state = Failed;
          return ERR(Void, Errors::bad_gateway);
        }
      } else {
        // Done writing: drop stdin from epoll, which also closes the pipe,
        // signalling EOF to the CGI script.
        dprintf(2, "closing stdin: fd=%d\n", _stdin->_fd);
        const int stdin = _stdin->_fd;
        _epoll.del_fd(_stdin);
        const int r = fcntl(stdin, F_GETFD);
        dprintf(2, "F_GETFD stdin(%d): %d (%s)\n", stdin, r, strerror(errno));
        _stdin = NULL;
        return OKV;
      }
    }
  }
  if (_stdout != NULL && ev->fd == _stdout) {
    // is_stdout
    dprintf(2, "stdout event: in=%d hup=%d\n", ev->in, ev->hup);
    if (ev->in || ev->hup || ev->rdhup) {
      char buffer[4096];
      const Result<ssize_t> bytes_read =
          _stdout->pipe_read(buffer, sizeof(buffer));
      if (bytes_read.has_value() && bytes_read.value() > 0) {
        _output.append(buffer, static_cast<size_t>(bytes_read.value()));
        return OKV;
      }
      if (!bytes_read.has_value() || bytes_read.value() < 0) {
        terminate_child(_pid);
        _pid = -1;
        _state = Failed;
        return ERR(Void, Errors::bad_gateway);
      }

      // bytes_read == 0: EOF, drain any remaining IO bookkeeping.
      _epoll.del_fd(_stdout);
      _stdout = NULL;
      if (_stdin != NULL) {
        dprintf(2, "closing stdin: fd=%d\n", _stdin->_fd);
        const int stdin = _stdin->_fd;
        _epoll.del_fd(_stdin);
        const int r = fcntl(stdin, F_GETFD);
        dprintf(2, "F_GETFD stdin(%d): %d (%s)\n", stdin, r, strerror(errno));
        _stdin = NULL;
      }
      _state = Done;

      int status = 0;
      if (!waitpid_nohang(_pid, &status)) {
        terminate_child(_pid);
        _state = Failed;
        _pid = -1;
        return ERR(Void, Errors::bad_gateway);
      }
      _pid = -1;

      if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        _state = Failed;
        return ERR(Void, Errors::bad_gateway);
      }

      return OKV;
    }

    if (ev->err) {
      terminate_child(_pid);
      _pid = -1;
      _state = Failed;
      return ERR(Void, Errors::bad_gateway);
    }
    return OKV;
  } else
    return OKV; // not for us
}

Result<std::string> CgiDelegate::poll() const {
  switch (_state) {
  case Done:
    return OK(std::string, _output);
  case Failed:
    return ERR(std::string, "CGI failed");
  default:
    return ERR(std::string, Errors::try_again);
  }
}

bool CgiDelegate::check_timeout() {
  if (remaining_ns() == 0) {
    _state = Failed;
    return true;
  }
  return false;
}

size_t CgiDelegate::remaining_ns() const {
  timespec now = {};

  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0;
  }
  const size_t now_ns =
      static_cast<size_t>(now.tv_sec * 1000000000 + now.tv_nsec);
  const size_t start_ns = static_cast<size_t>(_start_time.tv_sec * 1000000000 +
                                              _start_time.tv_nsec);
  if (now_ns >= start_ns + _timeout_ns) {
    return 0;
  } else {
    return start_ns + _timeout_ns - now_ns;
  }
}

CgiDelegate::~CgiDelegate() {
  if (_stdin != NULL) {
    _epoll.del_fd(_stdin);
    _stdin = NULL;
  }
  if (_stdout != NULL) {
    _epoll.del_fd(_stdout);
    _stdout = NULL;
  }
  if (_pid > 0) {
    terminate_child(_pid);
    _pid = -1;
  }
}
