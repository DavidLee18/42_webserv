#ifndef HTTP_1_1_H
#define HTTP_1_1_H

#include "json.h"
#include "map_record.h"
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
  enum HttpBodyType { HttpJson, HttpFormUrlEncoded } type;
  union HttpBodyValue {
    Json *json;
    MapRecord<std::string, std::string> *form;
  } value;

public:
  HttpBody(HttpBodyType t, HttpBodyValue v) : type(t), value(v) {}
};

class HttpReq {
  HttpMethod method;
  std::map<std::string, Json> headers;
  std::string path;
  HttpBody body;

public:
  HttpReq(HttpMethod m, std::string p, HttpBody b)
      : method(m), headers(), path(p), body(b) {}
};

#endif
