#include "webserv.h"

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

unsigned char to_upper(unsigned char c) {
  return static_cast<unsigned char>(std::toupper(static_cast<int>(c)));
}
