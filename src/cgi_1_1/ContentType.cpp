#include "ContentType.hpp"
#include "../core/Errors.hpp"

ContentType::ContentType(const ContentType::Type ty, std::string const& subty)
    : type(ty), subtype(subty), params() {}

ContentType::ContentType(ContentType const& other)
    : type(other.type), subtype(other.subtype), params(other.params) {}

ContentType& ContentType::operator=(const ContentType& other) {
    if (this != &other) {
        type    = other.type;
        subtype = other.subtype;
        params  = other.params;
    }
    return *this;
}

Result<Void> ContentType::add_param(const std::string& k,
                                    const std::string& v) {
    const std::map<std::string, std::string>::iterator iter = params.find(k);
    if (iter == params.end()) return ERR(Void, Errors::not_found);
    iter->second = v;
    return OKV;
}
