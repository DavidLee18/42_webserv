#ifndef ETC_META_VAR_HPP
#define ETC_META_VAR_HPP

#include <string>

class EtcMetaVar {

  public:
    enum Type { Http, Custom };
    EtcMetaVar(Type, std::string const&, std::string const&);
    EtcMetaVar(EtcMetaVar const&);
    EtcMetaVar&        operator=(EtcMetaVar const&);

    Type const&        get_type() const;
    std::string const& get_name() const;
    std::string const& get_value() const;

  private:
    Type        type;
    std::string name;
    std::string value;
};

#endif
