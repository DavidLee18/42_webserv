#ifndef HTTP_1_1_H
#define HTTP_1_1_H

#include "json.h"
#include <map>
#include <string>

enum HttpMethod {
  GET,
  HEAD,
  OPTIONS,
  POST,
  DELETE,
  PUT,
  CONNECT,
  TRACE,
  PATCH
};

class PartialString {
  enum PartStrType { Partial, Full } _kind;
  char *_part;

  PartialString() : _kind(PartialString::Partial), _part(NULL) {}

public:
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
  PartStrType const &kind() const { return _kind; }
  PartStrType &kind_mut() { return _kind; }
  char const *part() { return _part; }
  char *part_mut() { return _part; }
};

class HttpBody {
  enum HttpBodyType { Empty, HttpJson, HttpFormUrlEncoded, Html } _type;
  union HttpBodyValue {
    void *_null;
    Json *json;
    std::map<std::string, std::string> *form;
    std::string *html_raw;
  } _value;

public:
  HttpBody(HttpBodyType t, HttpBodyValue v) : _type(t), _value(v) {}
  HttpBody(const HttpBody &other) : _type(other._type) {
    switch (_type) {
    case Empty:
      _value._null = NULL;
      break;
    case HttpJson:
      _value.json = new Json(*other._value.json);
      break;
    case HttpFormUrlEncoded:
      _value.form = new std::map<std::string, std::string>(*other._value.form);
      break;
    case Html:
      _value.html_raw = new std::string(*other._value.html_raw);
      break;
    }
  }
  static HttpBody *empty() {
    return new HttpBody(Empty, (HttpBodyValue){._null = NULL});
  }
  static HttpBody *json(Json *json) {
    return new HttpBody(HttpJson, (HttpBodyValue){.json = json});
  }
  const HttpBodyType &type() const { return _type; }
  const HttpBodyValue &value() const { return _value; }
};

class HttpReq {
  HttpMethod _method;
  std::map<std::string, Json> _headers;
  std::string _path;
  HttpBody _body;

public:
  HttpReq(HttpMethod m, std::string p, HttpBody b)
      : _method(m), _headers(), _path(p), _body(b) {}
  const HttpMethod &method() const { return _method; }
  const std::map<std::string, Json> &headers() const { return _headers; }
  const std::string &path() const { return _path; }
  const HttpBody &body() const { return _body; }
  static Result<std::pair<HttpReq *, size_t> > parse(const char *, char);
};

#endif
