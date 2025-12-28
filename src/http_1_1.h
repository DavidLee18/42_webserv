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

struct PartialString {
  enum { Partial, Full } kind;
  char *part;
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

private:
  PartialString() : kind(PartialString::Partial), part(NULL) {}
};

struct HttpBody {
  enum { HttpJson, HttpFormUrlEncoded } type;
  union {
    Json json;
    MapRecord<std::string, std::string> *form;
  } value;
};

struct HttpRequest {
  HttpMethod method;
  std::map<std::string, Json> headers;
  std::string path;
  HttpBody body;
};

#endif
