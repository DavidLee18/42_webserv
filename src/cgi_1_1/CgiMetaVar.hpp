#ifndef CGI_META_VAR_HPP
#define CGI_META_VAR_HPP

#include "../server/Client.hpp"
#include "CgiAuthType.hpp"
#include "ContentType.hpp"
#include "EtcMetaVar.hpp"
#include "GatewayInterface.hpp"
#include "ServerName.hpp"
#include "ServerProtocol.hpp"
#include "ServerSoftware.hpp"

#include <list>
#include <string>

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
        CgiAuthType*            auth_type;
        unsigned int            content_length;
        ContentType*            content_type;
        GatewayInterface        gateway_interface;
        std::list<std::string>* path_info;
        std::string*            path_translated;
        std::string*            query_string;
        unsigned char           remote_addr[4];
        std::list<std::string>* remote_host;
        std::string*            remote_ident;
        std::string*            remote_user;
        Request::Method         request_method;
        std::list<std::string>* script_name;
        ServerName*             server_name;
        unsigned short          server_port;
        ServerProtocol          server_protocol;
        ServerSoftware          server_software;
        EtcMetaVar*             etc_val;
    };

    class Parser {
        virtual void phantom() = 0;
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_auth_type(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_content_length(const std::string& raw);
        static Result<std::pair<CgiMetaVar, size_t> >
            parse_content_type(std::string);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_gateway_interface(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_path_info(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_path_translated(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_query_string(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_addr(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_host(const std::string& raw);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_ident(const std::string& raw);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_user(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_request_method(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_script_name(const std::string&);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_name(const std::string& raw);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_port(const std::string& raw);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_protocol(const std::string& raw);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_software(const std::string& raw);
        static Result<std::pair<CgiMetaVar, size_t> >
        parse_custom_var(const std::string& name, const std::string& value);

      public:
        static Result<std::pair<CgiMetaVar, size_t> > parse(std::string const&,
                                                            std::string const&);
    };

    friend class CgiInput;

    CgiMetaVar(CgiMetaVar const&);
    CgiMetaVar& operator=(CgiMetaVar const&);
    ~CgiMetaVar();

    Name const& get_name() const;
    Val const&  get_val() const;

  private:
    Name name;
    Val  val;

    CgiMetaVar(Name, Val);

    static CgiMetaVar auth_type(const CgiAuthType&);
    static CgiMetaVar content_length(unsigned int);
    static CgiMetaVar content_type(const ContentType&);
    static CgiMetaVar gateway_interface(GatewayInterface);
    static CgiMetaVar path_info(const std::list<std::string>&);
    static CgiMetaVar path_translated(const std::string&);
    static CgiMetaVar query_string(const std::string&);
    static CgiMetaVar remote_addr(unsigned char, unsigned char, unsigned char,
                                  unsigned char);
    static CgiMetaVar remote_host(const std::list<std::string>&);
    static CgiMetaVar remote_ident(const std::string&);
    static CgiMetaVar remote_user(const std::string&);
    static CgiMetaVar request_method(Request::Method);
    static CgiMetaVar script_name(const std::list<std::string>&);
    static CgiMetaVar server_name(const ServerName&);
    static CgiMetaVar server_port(unsigned short);
    static CgiMetaVar server_protocol(ServerProtocol);
    static CgiMetaVar server_software(ServerSoftware);
    static CgiMetaVar custom_var(EtcMetaVar::Type, const std::string&,
                                 const std::string&);
};

#endif
