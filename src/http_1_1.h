#ifndef HTTP_1_1_H
#define HTTP_1_1_H

#include "result.h"
#include <map>
#include <string>
#include <vector>

class FileDescriptor;

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

template <typename K, typename V> struct MapRecord {
  K key;
  V value;
  MapRecord(K k, V v) : key(k), value(v) {}
};

struct Json {
  enum { JsonNull, JsonBool, JsonNum, JsonStr, JsonArr, JsonObj } type;
  union JsonValue {
    void *_null;
    bool _bool;
    long double num;
    char *_str;
    Json *arr;
    MapRecord<std::string, Json> *obj;
  } value;
  static Result<Json> from_raw(const char *raw);

  static Json *null();
  static Json *_bool(bool);
  static Json *num(long double);
  static Json *str(char *);
  static Json *arr(Json *);
  static Json *obj(MapRecord<std::string, Json> *);

private:
  static Result<Json> null_or_undef(const char *);
  static Result<Json> _boolean(const char *);
  static Result<Json> _num(const char *);
  static Result<Json> _str(const char *);
  static Result<Json> _arr(const char *);
  static Result<Json> _obj(const char *);
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
