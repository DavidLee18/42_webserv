#include "webserv.h"

Result<Void> ContentType::add_param(std::string k, std::string v) {
  std::map<std::string, std::string>::iterator iter = params.find(k);
  if (iter == params.end())
    return ERR(Void, Errors::not_found);
  iter->second = v;
  return OKV;
}

ServerName *ServerName::host(std::string raw) {
  std::stringstream ss(raw);
  std::list<std::string> *hostparts = new std::list<std::string>();
  std::string part;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    hostparts->push_back(part);
    std::getline(ss, part, '.');
  }
  return new ServerName(Host, (ServerNameVal){.host_name = hostparts});
}

ServerName *ServerName::ipv4(unsigned char b1, unsigned char b2,
                             unsigned char b3, unsigned char b4) {
  return new ServerName(Ipv4, (ServerNameVal){.ipv4 = {b1, b2, b3, b4}});
}

Result<ServerName *> ServerName::ipv4(std::string raw) {
  std::stringstream ss(raw);
  std::vector<std::string> *ips = new std::vector<std::string>();
  std::string part;
  std::getline(ss, part, '.');
  while (ss && !ss.eof()) {
    ips->push_back(part);
    std::getline(ss, part, '.');
  }
  if (ips->size() != 4)
    return ERR(ServerName *, Errors::invalid_format);
  unsigned char addrs[4];
  for (size_t i = 0; i < 4; i++) {
    if ((*ips)[i].size() < 1 || (*ips)[i].size() > 3)
      return ERR(ServerName *, Errors::invalid_format);
    for (size_t j = 0; j < (*ips)[i].size(); j++) {
      if ('0' < (*ips)[i][j] || (*ips)[i][j] > '9')
        return ERR(ServerName *, Errors::invalid_format);
    }
    addrs[i] = static_cast<unsigned char>(std::atoi((*ips)[i].c_str()));
  }
  ServerName **res = new ServerName *;
  *res = new ServerName(
      Ipv4, (ServerNameVal){.ipv4 = {addrs[0], addrs[1], addrs[2], addrs[3]}});
  return OK(ServerName *, res);
}
