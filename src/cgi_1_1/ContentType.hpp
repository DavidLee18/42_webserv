#ifndef CONTENT_TYPE_HPP
#define CONTENT_TYPE_HPP

#include "../core/Result.hpp"

#include <map>
#include <string>

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
    ContentType(Type, std::string const&);
    ContentType(ContentType const&);
    ContentType& operator=(ContentType const&);
    Result<Void> add_param(const std::string& k, const std::string& v);

    Type         type;
    std::string  subtype;
    std::map<std::string, std::string> params;
};

#endif
