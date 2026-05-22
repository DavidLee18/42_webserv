#include "EtcMetaVar.hpp"

EtcMetaVar::EtcMetaVar(const EtcMetaVar::Type ty, std::string const& n,
                       std::string const& v)
    : type(ty), name(n), value(v) {}

EtcMetaVar::EtcMetaVar(EtcMetaVar const& other)
    : type(other.type), name(other.name), value(other.value) {}

EtcMetaVar& EtcMetaVar::operator=(const EtcMetaVar& other) {
    if (this != &other) {
        type  = other.type;
        name  = other.name;
        value = other.value;
    }
    return *this;
}

EtcMetaVar::Type const& EtcMetaVar::get_type() const { return type; }
std::string const&      EtcMetaVar::get_name() const { return name; }
std::string const&      EtcMetaVar::get_value() const { return value; }
