#include "CgiAuthType.hpp"

CgiAuthType::CgiAuthType(const CgiAuthType::Type type)
    : _type(type), _other(NULL) {}

CgiAuthType::CgiAuthType(const CgiAuthType::Type type, const std::string& other)
    : _type(type), _other(new std::string(other)) {}

CgiAuthType::CgiAuthType(const CgiAuthType& other) : _type(other._type) {
    if (other._other != NULL) {
        _other = new std::string(*other._other);
    } else {
        _other = NULL;
    }
}

CgiAuthType& CgiAuthType::operator=(const CgiAuthType& other) {
    if (this != &other) {
        delete _other;
        _type = other._type;
        if (other._other != NULL) {
            _other = new std::string(*other._other);
        } else {
            _other = NULL;
        }
    }
    return *this;
}

CgiAuthType::~CgiAuthType() { delete _other; }

CgiAuthType::Type const& CgiAuthType::type() const { return _type; }

std::string const*       CgiAuthType::other() const { return _other; }
