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
  enum { Partial, Full } kind;
  char *part;

  PartialString() : kind(PartialString::Partial), part(NULL) {}

public:
  static PartialString *partial(char *input) {
    PartialString *p = new PartialString();
    p->part = input;
    return p;
  }
  static PartialString *full(char *input) {
    PartialString *p = new PartialString();
    p->kind = PartialString::Full;
    p->part = input;
    return p;
  }
};

class HttpBody {
  enum HttpBodyType { Empty, HttpJson, HttpFormUrlEncoded, Html } type;
  union HttpBodyValue {
    void *_null;
    Json *json;
    std::map<std::string, std::string> *form;
    std::string *html_raw;
  } value;

public:
  HttpBody(HttpBodyType t, HttpBodyValue v) : type(t), value(v) {}
  HttpBody(const HttpBody &other) : type(other.type) {
    switch (type) {
    case Empty:
      value._null = NULL;
      break;
    case HttpJson:
      value.json = new Json(*other.value.json);
      break;
    case HttpFormUrlEncoded:
      value.form = new std::map<std::string, std::string>(*other.value.form);
      break;
    case Html:
      value.html_raw = new std::string(*other.value.html_raw);
      break;
    }
  }
  const HttpBodyType &ty() { return type; }
  const HttpBodyValue &val() { return value; }
};

class HttpReq {
  HttpMethod method;
  std::map<std::string, Json> headers;
  std::string path;
  HttpBody body;

public:
  HttpReq(HttpMethod m, std::string p, HttpBody b)
      : method(m), headers(), path(p), body(b) {}
  const HttpMethod &mthd() { return method; }
  const std::map<std::string, Json> &hds() { return headers; }
  const std::string &pth() { return path; }
  const HttpBody &bdy() { return body; }
};

#endif
