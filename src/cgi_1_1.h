#ifndef CGI_1_1_H
#define CGI_1_1_H

#include "config/RouteRule_CGI.hpp"
#include "errors.h"
#include "result.h"
#include "server/Client.hpp"
#include "server/Response.hpp"
#include <cstddef>
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <sys/types.h>
#include <vector>

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
  CgiAuthType(Type, std::string const &);
  CgiAuthType(const CgiAuthType &);
  CgiAuthType &operator=(const CgiAuthType &);
  ~CgiAuthType();
  Type const &type() const;
  std::string const *other() const;

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
  Result<Void> add_param(const std::string &k, const std::string &v);

  Type type;
  std::string subtype;
  std::map<std::string, std::string> params;
};

enum GatewayInterface { Cgi_1_1 };

class ServerName {
public:
  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<ServerName, size_t> >
    parse_host(const std::string &raw);
    static Result<std::pair<ServerName, size_t> >
    parse_ipv4(const std::string &raw);

  public:
    static Result<std::pair<ServerName, size_t> > parse(const std::string &raw);
  };

  enum Type {
    Host,
    Ipv4,
  };

  union Val {
    std::list<std::string> *host_name;
    unsigned char ipv4[4];
  };

  ServerName(ServerName const &);
  ServerName &operator=(ServerName const &);
  ~ServerName();

  Type const &get_type() const;
  Val const &get_val() const;

private:
  Type type;
  Val val;

  ServerName(Type, Val);

  static ServerName host(const std::list<std::string> &hostparts);
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
    std::string *query_string;
    unsigned char remote_addr[4];
    std::list<std::string> *remote_host;
    std::string *remote_ident;
    std::string *remote_user;
    Request::Method request_method;
    std::list<std::string> *script_name;
    ServerName *server_name;
    unsigned short server_port;
    ServerProtocol server_protocol;
    ServerSoftware server_software;
    EtcMetaVar *etc_val;
  };

  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_auth_type(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_content_length(const std::string &raw);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_content_type(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_gateway_interface(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_path_info(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_path_translated(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_query_string(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_remote_addr(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_remote_host(const std::string &raw);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_remote_ident(const std::string &raw);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_remote_user(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_request_method(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_script_name(const std::string &);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_server_name(const std::string &raw);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_server_port(const std::string &raw);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_server_protocol(const std::string &raw);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_server_software(const std::string &raw);
    static Result<std::pair<CgiMetaVar, size_t> >
    parse_custom_var(const std::string &name, const std::string &value);

  public:
    static Result<std::pair<CgiMetaVar, size_t> > parse(std::string const &,
                                                        std::string const &);
  };

  friend class CgiInput;

  CgiMetaVar(CgiMetaVar const &);
  CgiMetaVar &operator=(CgiMetaVar const &);
  ~CgiMetaVar();

  Name const &get_name() const;
  Val const &get_val() const;

private:
  Name name;
  Val val;

  CgiMetaVar(Name, Val);

  static CgiMetaVar auth_type(const CgiAuthType &);
  static CgiMetaVar content_length(unsigned int);
  static CgiMetaVar content_type(const ContentType &);
  static CgiMetaVar gateway_interface(GatewayInterface);
  static CgiMetaVar path_info(const std::list<std::string> &);
  static CgiMetaVar path_translated(const std::string &);
  static CgiMetaVar query_string(const std::string &);
  static CgiMetaVar remote_addr(unsigned char, unsigned char, unsigned char,
                                unsigned char);
  static CgiMetaVar remote_host(const std::list<std::string> &);
  static CgiMetaVar remote_ident(const std::string &);
  static CgiMetaVar remote_user(const std::string &);
  static CgiMetaVar request_method(Request::Method);
  static CgiMetaVar script_name(const std::list<std::string> &);
  static CgiMetaVar server_name(const ServerName &);
  static CgiMetaVar server_port(unsigned short);
  static CgiMetaVar server_protocol(ServerProtocol);
  static CgiMetaVar server_software(ServerSoftware);
  static CgiMetaVar custom_var(EtcMetaVar::Type, const std::string &,
                               const std::string &);
};

class CgiInput {
  std::vector<CgiMetaVar> mvars;
  std::string req_body;

  CgiInput();
  CgiInput(std::vector<CgiMetaVar> const &, const std::string &);
  explicit CgiInput(Request const &);

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
  Result<Void> add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
};

class CgiDelegate {
public:
  enum State {
    NotRegistered,
    Waiting,
    Failed,
    Done,
    Reaping,
  };

  static Result<CgiDelegate>
  from_req(Request const &, EPoll &, RouteRule_CGI const &,
           std::map<std::string, std::string> const &);

  CgiDelegate(const CgiDelegate &);
  CgiDelegate &operator=(const CgiDelegate &) throw(std::logic_error);

  // Phase 1: create pipes, fork, register the pipe fds with epoll.
  // After this returns OK, the main event loop will deliver events on the
  // registered fds; the caller must route them to handle_event().
  Result<Void>
  register_(std::map<FileDescriptor const *,
                     std::pair<FileDescriptor const *, CgiDelegate *> > &cgis,
            FileDescriptor const *client_fd);

  // Phase 2: process a single epoll event for this CGI. Performs
  // non-blocking IO only; never calls epoll->wait(). Returns an error if
  // the CGI fails, in which case is_done() also becomes true.
  Result<Void> handle_event(const Event *);

  Result<std::string> poll() const;

  bool check_timeout();

  size_t remaining_ns() const;

  bool wait_or_reap();

  ~CgiDelegate();

private:
  CgiInput _env;
  std::string _script_path;
  std::string _interpreter;
  const Request _req;
  EPoll &_epoll;
  pid_t _pid;
  FileDescriptor *_stdin;
  FileDescriptor *_stdout;
  size_t _total_written;
  std::string _output;
  State _state;
  timespec _start_time;
  size_t _timeout_ns;

  CgiDelegate(Request const &, EPoll &);
};

unsigned char to_upper(unsigned char);

#endif
