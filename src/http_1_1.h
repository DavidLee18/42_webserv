#ifndef HTTP_1_1_H
#define HTTP_1_1_H

#include "json.h"
#include <map>
#include <string>

class Http {
  virtual void phantom() = 0;

public:
  enum Method { GET, HEAD, OPTIONS, POST, DELETE, PUT, CONNECT, TRACE, PATCH };

  class PartialString {
  public:
    enum Type { Partial, Full };
    static PartialString *partial(char *input) {
      PartialString *p = new PartialString();
      p->_part = input;
      return p;
    }
    static PartialString *full(char *input) {
      PartialString *p = new PartialString();
      p->_kind = PartialString::Full;
      p->_part = input;
      return p;
    }
    Type const &kind() const { return _kind; }
    Type &kind_mut() { return _kind; }
    char const *part() { return _part; }
    char *part_mut() { return _part; }

  private:
    Type _kind;
    char *_part;

    PartialString() : _kind(Partial), _part(NULL) {}
  };

  class Body {
  public:
    enum Type { Empty, HttpJson, HttpFormUrlEncoded, Html };
    union Value {
      void *_null;
      Json *json;
      std::map<std::string, std::string> *form;
      std::string *html_raw;
    };
    Body(Type t, Value v) : _type(t), _value(v) {}
    Body(const Body &other) : _type(other._type) {
      switch (_type) {
      case Empty:
        _value._null = NULL;
        break;
      case HttpJson:
        _value.json = new Json(*other._value.json);
        break;
      case HttpFormUrlEncoded:
        _value.form =
            new std::map<std::string, std::string>(*other._value.form);
        break;
      case Html:
        _value.html_raw = new std::string(*other._value.html_raw);
        break;
      }
    }
    static Body *empty() { return new Body(Empty, (Value){._null = NULL}); }
    static Body *json(Json *json) {
      return new Body(HttpJson, (Value){.json = json});
    }
    const Type &type() const { return _type; }
    const Value &value() const { return _value; }

  private:
    Type _type;
    Value _value;
  };

  class Request {
    Method _method;
    std::map<std::string, Json> _headers;
    std::string _path;
    Body _body;

  public:
    Request(Method m, std::string p, Body b)
        : _method(m), _headers(), _path(p), _body(b) {}
    const Method &method() const { return _method; }
    const std::map<std::string, Json> &headers() const { return _headers; }
    const std::string &path() const { return _path; }
    const Body &body() const { return _body; }
    static Result<std::pair<Request *, size_t> > parse(const char *, char);
  };
};
#endif
