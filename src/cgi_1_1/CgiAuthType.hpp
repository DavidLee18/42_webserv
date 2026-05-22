#ifndef CGI_AUTH_TYPE_HPP
#define CGI_AUTH_TYPE_HPP

#include <string>

class CgiAuthType {
  public:
    enum Type {
        Basic,
        Digest,
        CgiAuthOther,
    };
    explicit CgiAuthType(Type);
    CgiAuthType(Type, std::string const&);
    CgiAuthType(const CgiAuthType&);
    CgiAuthType& operator=(const CgiAuthType&);
    ~CgiAuthType();
    Type const&        type() const;
    std::string const* other() const;

  private:
    Type         _type;
    std::string* _other;
};

#endif
