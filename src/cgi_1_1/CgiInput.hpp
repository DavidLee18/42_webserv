#ifndef CGIINPUT_HPP
#define CGIINPUT_HPP

#include "CgiMetaVar.hpp"
#include <string>
#include <vector>

class CgiInput {
    std::vector<CgiMetaVar> mvars;
    std::string             req_body;

    CgiInput();
    CgiInput(std::vector<CgiMetaVar> const&, const std::string&);
    explicit CgiInput(Request const&);

  public:
    class Parser {
        virtual void phantom() = 0;

      public:
        static Result<CgiInput> parse(Request const&);
    };

    friend class Parser;
    friend class CgiDelegate;

    CgiInput(const CgiInput&);
    CgiInput&    operator=(const CgiInput&);
    Result<Void> add_mvar(std::string const&, std::string const&);
    char**       to_envp() const;
};

#endif
