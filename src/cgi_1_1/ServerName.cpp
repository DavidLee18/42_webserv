#include "ServerName.hpp"
#include "../core/Errors.hpp"
#include <cstdlib>
#include <sstream>
#include <vector>

ServerName ServerName::host(const std::list<std::string>& hostparts) {
    return ServerName(
        Host,
        (ServerName::Val){.host_name = new std::list<std::string>(hostparts)});
}

ServerName ServerName::ipv4(const unsigned char b1, const unsigned char b2,
                            const unsigned char b3, const unsigned char b4) {
    return ServerName(Ipv4, (ServerName::Val){.ipv4 = {b1, b2, b3, b4}});
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse_host(const std::string& raw) {
    std::stringstream            ss(raw);
    const std::list<std::string> parts;
    std::string                  part;
    const bool                   dom_end = false;
    size_t                       j       = 0;
    std::getline(ss, part, '.');
    while (ss && !ss.eof()) {
        if (part.empty())
            return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
        if (!std::isalnum(static_cast<unsigned char>(part[0])) ||
            !std::isalpha(static_cast<unsigned char>(part[0])))
            return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
        j++;
        if (part.size() > 1) {
            for (size_t i = 1; i < part.size() - 1; i++) {
                if (!std::isalnum(static_cast<unsigned char>(part[i])) &&
                    part[i] != '-')
                    return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
            }
            if (!std::isalnum(
                    static_cast<unsigned char>(part[part.size() - 1])))
                return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
        }
        j += part.size();
        std::getline(ss, part, '.');
    }
    if (!ss.eof()) return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    if (part.empty())
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    if ((!dom_end && !std::isalnum(static_cast<unsigned char>(part[0]))) ||
        !std::isalpha(static_cast<unsigned char>(part[0])))
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    j++;
    if (part.size() > 1) {
        for (size_t i = 1; i < part.size() - 1; i++) {
            if (!std::isalnum(static_cast<unsigned char>(part[i])) &&
                part[i] != '-')
                return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
        }
        if (!std::isalnum(static_cast<unsigned char>(part[part.size() - 1])))
            return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    }
    return OK_PAIR(ServerName, size_t, ServerName::host(parts),
                   j + part.size());
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse_ipv4(const std::string& raw) {
    std::stringstream          ss(raw);
    std::vector<unsigned char> addrs;
    std::string                part;
    size_t                     i = 0;
    std::getline(ss, part, '.');
    while (ss && !ss.eof()) {
        if (part.empty() || part.size() > 3)
            return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
        for (size_t j = 0; j < part.size(); j++) {
            if (part[j] < '0' || part[j] > '9')
                return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
        }
        i += part.size();
        char*         end      = NULL;
        unsigned long part_num = std::strtoul(part.c_str(), &end, 10);
        if (end == NULL || *end != '\0' || part_num > 255)
            return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
        addrs.push_back(static_cast<unsigned char>(part_num));
        std::getline(ss, part, '.');
    }
    if (!ss.eof()) return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    if (part.empty() || part.size() > 3)
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    for (size_t j = 0; j < part.size(); j++) {
        if (part[j] < '0' || part[j] > '9')
            return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    }
    char*         end      = NULL;
    unsigned long part_num = std::strtoul(part.c_str(), &end, 10);
    if (end == NULL || *end != '\0' || part_num > 255)
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    addrs.push_back(static_cast<unsigned char>(part_num));
    if (addrs.size() != 4)
        return ERR_PAIR(ServerName, size_t, Errors::invalid_format);
    return OK_PAIR(ServerName, size_t,
                   ServerName::ipv4(addrs[0], addrs[1], addrs[2], addrs[3]),
                   i + part.size());
}

Result<std::pair<ServerName, size_t> >
ServerName::Parser::parse(const std::string& raw) {
    Result<std::pair<ServerName, size_t> > res = parse_host(raw);
    if (res.error().empty()) return res;
    return parse_ipv4(raw);
}

ServerName::ServerName(const ServerName::Type ty, const ServerName::Val v)
    : type(ty), val(v) {}

ServerName::ServerName(const ServerName& other) : type(other.type), val() {
    if (type == Host) {
        val.host_name = new std::list<std::string>(*other.val.host_name);
    } else {
        val.ipv4[0] = other.val.ipv4[0];
        val.ipv4[1] = other.val.ipv4[1];
        val.ipv4[2] = other.val.ipv4[2];
        val.ipv4[3] = other.val.ipv4[3];
    }
}

ServerName::~ServerName() {
    if (type == ServerName::Host) { delete val.host_name; }
}

ServerName& ServerName::operator=(const ServerName& other) {
    if (this != &other) {
        if (type == Host) { delete val.host_name; }

        type = other.type;
        if (type == Host) {
            val.host_name = new std::list<std::string>(*other.val.host_name);
        } else {
            val.ipv4[0] = other.val.ipv4[0];
            val.ipv4[1] = other.val.ipv4[1];
            val.ipv4[2] = other.val.ipv4[2];
            val.ipv4[3] = other.val.ipv4[3];
        }
    }
    return *this;
}

ServerName::Type const& ServerName::get_type() const { return type; }
ServerName::Val const&  ServerName::get_val() const { return val; }

std::ostream&           operator<<(std::ostream& os, ServerName const& srvn) {
    if (srvn.get_type() == ServerName::Host) {
        bool first = true;
        for (std::list<std::string>::const_iterator it =
                 srvn.get_val().host_name->begin();
             it != srvn.get_val().host_name->end(); ++it) {
            if (!first) os << '.';
            os << *it;
            first = false;
        }
        return os;
    } else {
        os << static_cast<int>(srvn.get_val().ipv4[0]) << "."
           << static_cast<int>(srvn.get_val().ipv4[1]) << "."
           << static_cast<int>(srvn.get_val().ipv4[2]) << "."
           << static_cast<int>(srvn.get_val().ipv4[3]);
        return os;
    }
}
