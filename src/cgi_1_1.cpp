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
      (ServerNameVal){.host_name = new std::list<std::string>(hostparts)});
}

ServerName *ServerName::ipv4(unsigned char b1, unsigned char b2,
                             unsigned char b3, unsigned char b4) {
  return new ServerName(Ipv4, (ServerNameVal){.ipv4 = {b1, b2, b3, b4}});
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
ServerName::Parser::parse(std::string raw) {}

// CgiMetaVar *CgiMetaVar::auth_type(CgiAuthType ty) {
//   return new CgiMetaVar(AUTH_TYPE, (CgiMetaVar_){.auth_type = ty});
// }

// CgiMetaVar *CgiMetaVar::content_length(unsigned int l) {
//   return new CgiMetaVar(CONTENT_LENGTH, (CgiMetaVar_){.content_length = l});
// }

// CgiMetaVar *CgiMetaVar::content_type(ContentType ty) {
//   return new CgiMetaVar(CONTENT_TYPE,
//                         (CgiMetaVar_){.content_type = new ContentType(ty)});
// }

// CgiMetaVar *CgiMetaVar::gateway_interface(GatewayInterface i) {
//   return new CgiMetaVar(GATEWAY_INTERFACE,
//                         (CgiMetaVar_){.gateway_interface = i});
// }

// Result<CgiMetaVar *> CgiMetaVar::path_info(std::string raw) {}
