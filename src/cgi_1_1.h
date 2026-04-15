#ifndef CGI_1_1_H
#define CGI_1_1_H

#include "errors.h"
#include "http_1_1.h"
#include "result.h"
#include "server/Client.hpp"
#include <cstddef>
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <sys/types.h>

class CgiInput;
class EPoll;
class Event;
class FileDescriptor;

class CgiAuthType {
public:
  enum Type {
    Basic,
    Digest,
    CgiAuthOther,
  };
  explicit CgiAuthType(Type);
  CgiAuthType(Type, std::string);
  CgiAuthType(const CgiAuthType &);
  CgiAuthType &operator=(const CgiAuthType &);
  ~CgiAuthType();
  Type const &type();
  std::string const *other();

private:
  Type _type;
  std::string *_other;
};

class ContentType {
public:
  enum Type {
    application,
    audio,
    example,
    font,
    haptics,
    image,
    message,
    model,
    multipart,
    text,
    video
  };
  ContentType(Type, std::string const &);
  ContentType(ContentType const &);
  ContentType &operator=(ContentType const &);
  Result<Void> add_param(std::string, std::string);

public:
  Type type;
  std::string subtype;
  std::map<std::string, std::string> params;
};

enum GatewayInterface { Cgi_1_1 };

class ServerName {
public:
  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<ServerName, size_t> > parse_host(std::string);
    static Result<std::pair<ServerName, size_t> > parse_ipv4(std::string);

  public:
    static Result<std::pair<ServerName, size_t> > parse(std::string);
  };

  enum Type {
    Host,
    Ipv4,
  };

  union Val {
    std::list<std::string> *host_name;
    unsigned char ipv4[4];
  };

public:
  ServerName(ServerName const &);
  ServerName &operator=(ServerName const &);
  ~ServerName();

  Type const &get_type() const;
  Val const &get_val() const;

private:
  Type type;
  Val val;

  ServerName(Type, Val);

  static ServerName host(std::list<std::string>);
  static ServerName ipv4(unsigned char, unsigned char, unsigned char,
                         unsigned char);
};

std::ostream &operator<<(std::ostream &, ServerName const &);

enum ServerProtocol { Http_1_1 };

enum ServerSoftware { Webserv };

class EtcMetaVar {

public:
  enum Type { Http, Custom };
  EtcMetaVar(Type, std::string const &, std::string const &);
  EtcMetaVar(EtcMetaVar const &);
  EtcMetaVar &operator=(EtcMetaVar const &);

  Type const &get_type() const;
  std::string const &get_name() const;
  std::string const &get_value() const;

private:
  Type type;
  std::string name;
  std::string value;
};

class CgiMetaVar {
public:
  enum Name {
    AUTH_TYPE,
    CONTENT_LENGTH,
    CONTENT_TYPE,
    GATEWAY_INTERFACE,
    PATH_INFO,
    PATH_TRANSLATED,
    QUERY_STRING,
    REMOTE_ADDR,
    REMOTE_HOST,
    REMOTE_IDENT,
    REMOTE_USER,
    REQUEST_METHOD,
    SCRIPT_NAME,
    SERVER_NAME,
    SERVER_PORT,
    SERVER_PROTOCOL,
    SERVER_SOFTWARE,
    X_, // Custom var
  };

  union Val {
    CgiAuthType *auth_type;
    unsigned int content_length;
    ContentType *content_type;
    GatewayInterface gateway_interface;
    std::list<std::string> *path_info;
    std::string *path_translated;
    std::map<std::string, std::string> *query_string;
    unsigned char remote_addr[4];
    std::list<std::string> *remote_host;
    std::string *remote_ident;
    std::string *remote_user;
    Http::Method request_method;
    std::list<std::string> *script_name;
    ServerName *server_name;
    unsigned short server_port;
    ServerProtocol server_protocol;
    ServerSoftware server_software;
    EtcMetaVar *etc_val;
  };

  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<CgiMetaVar, size_t> > parse_auth_type(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_content_length(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_content_type(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_gateway_interface(std::string);
    static Result<std::pair<CgiMetaVar, size_t> > parse_path_info(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_path_translated(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_query_string(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_addr(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_host(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_ident(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_user(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_request_method(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_script_name(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_name(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_port(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_protocol(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_software(std::string);
    static Result<std::pair<CgiMetaVar, size_t> > parse_custom_var(std::string,
                                                                   std::string);

  public:
    static Result<std::pair<CgiMetaVar, size_t> > parse(std::string const &,
                                                        std::string const &);
  };

  friend class CgiInput;

public:
  CgiMetaVar(CgiMetaVar const &);
  CgiMetaVar &operator=(CgiMetaVar const &);
  ~CgiMetaVar();

  Name const &get_name() const;
  Val const &get_val() const;

private:
  Name name;
  Val val;

  CgiMetaVar(Name, Val);

  static CgiMetaVar auth_type(CgiAuthType);
  static CgiMetaVar content_length(unsigned int);
  static CgiMetaVar content_type(const ContentType &);
  static CgiMetaVar gateway_interface(GatewayInterface);
  static CgiMetaVar path_info(std::list<std::string>);
  static CgiMetaVar path_translated(std::string);
  static CgiMetaVar query_string(std::map<std::string, std::string>);
  static CgiMetaVar remote_addr(unsigned char, unsigned char, unsigned char,
                                unsigned char);
  static CgiMetaVar remote_host(std::list<std::string>);
  static CgiMetaVar remote_ident(std::string);
  static CgiMetaVar remote_user(std::string);
  static CgiMetaVar request_method(Http::Method);
  static CgiMetaVar script_name(std::list<std::string>);
  static CgiMetaVar server_name(ServerName);
  static CgiMetaVar server_port(unsigned short);
  static CgiMetaVar server_protocol(ServerProtocol);
  static CgiMetaVar server_software(ServerSoftware);
  static CgiMetaVar custom_var(EtcMetaVar::Type, std::string, std::string);
};

class CgiInput {
  std::vector<CgiMetaVar> mvars;
  std::string req_body;

private:
  CgiInput();
  CgiInput(std::vector<CgiMetaVar> const &, std::string);
  CgiInput(Request const &);

public:
  class Parser {
    virtual void phantom() = 0;

  public:
    static Result<CgiInput> parse(Request const &);
  };

  friend class Parser;
  friend class CgiDelegate;

  CgiInput(const CgiInput &);
  CgiInput &operator=(const CgiInput &);
  void add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
};

class CgiDelegate {
  CgiInput _env;
  std::string _script_path;
  const Request &_req;
  EPoll &_epoll;
  pid_t _pid;
  FileDescriptor *_stdin;
  FileDescriptor *_stdout;
  size_t _total_written;
  Result<std::string> _res;

  CgiDelegate(Request const &, EPoll &);

public:
  Result<CgiDelegate> from_req(Request const &, EPoll &, std::string const &);

  // Phase 1: create pipes, fork, register the pipe fds with epoll.
  // After this returns OK, the main event loop will deliver events on the
  // registered fds; the caller must route them to handle_event().
  Result<Void> register_();

  // Phase 2: process a single epoll event for this CGI. Performs
  // non-blocking IO only; never calls epoll->wait(). Returns an error if
  // the CGI fails, in which case is_done() also becomes true.
  Result<Void> handle_event(const Event *);

  Result<std::string> poll();

  ~CgiDelegate();
};

unsigned char to_upper(unsigned char);

#endif
