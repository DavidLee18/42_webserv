#ifndef SERVER_NAME_HPP
#define SERVER_NAME_HPP

#include "../core/Result.hpp"
#include <list>
#include <string>

class ServerName {
  public:
    class Parser {
        virtual void phantom() = 0;
        static Result<std::pair<ServerName, size_t> >
        parse_host(const std::string& raw);
        static Result<std::pair<ServerName, size_t> >
        parse_ipv4(const std::string& raw);

      public:
        static Result<std::pair<ServerName, size_t> >
        parse(const std::string& raw);
    };

    enum Type {
        Host,
        Ipv4,
    };

    union Val {
        std::list<std::string>* host_name;
        unsigned char           ipv4[4];
    };

    ServerName(ServerName const&);
    ServerName& operator=(ServerName const&);
    ~ServerName();

    Type const& get_type() const;
    Val const&  get_val() const;

  private:
    Type type;
    Val  val;

    ServerName(Type, Val);

    static ServerName host(const std::list<std::string>& hostparts);
    static ServerName ipv4(unsigned char, unsigned char, unsigned char,
                           unsigned char);
};

std::ostream& operator<<(std::ostream&, ServerName const&);

#endif
