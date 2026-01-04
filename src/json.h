#ifndef JSON_H
#define JSON_H

#include "map_record.h"
#include "result.h"
#include "vec.h"
#include <string>

class Json;

class Json {
  enum JsonType { JsonNull, JsonBool, JsonNum, JsonStr, JsonArr, JsonObj } type;
  union JsonValue {
    void *_null;
    bool _bool;
    long double num;
    std::string *_str;
    Vec<Json> *arr;
    Vec<MapRecord<std::string, Json> > *obj;
  } value;

public:
  static Json *null();
  static Json *_bool(bool);
  static Json *num(long double);
  static Json *str(std::string);
  static Json *arr(Vec<Json>);
  static Json *obj(Vec<MapRecord<std::string, Json> >);

  Json() : type(JsonNull), value((JsonValue){._null = NULL}) {}
  Json(JsonType ty, JsonValue val) : type(ty), value(val) {}
  Json(const Json &other);
  const JsonType &ty() { return type; }
  const JsonValue &val() { return value; }
  friend std::ostream &operator<<(std::ostream &os, Json &js);
  
class Parser {
    virtual void phantom() = 0;
    static Result<MapRecord<Json *, size_t> > null_or_undef(const char *);
    static Result<MapRecord<Json *, size_t> > _boolean(const char *);
    static Result<MapRecord<Json *, size_t> > _num(const char *);
    static Result<MapRecord<Json *, size_t> > _str(const char *);
    static Result<MapRecord<Json *, size_t> > _arr(const char *);
    static Result<MapRecord<Json *, size_t> > _obj(const char *);

  public:
    static Result<MapRecord<Json *, size_t> > parse(const char *);
  };
};

#define TRY_PARSE(f, r, rs, i, s)                                              \
  r = f(s + i);                                                                \
  if (r.error().empty()) {                                                     \
    rs.push(*r.value()->key);                                                  \
    delete r.value()->key;                                                     \
    i += r.value()->value;                                                     \
    continue;                                                                  \
  }

#define TRY_PARSE_REC(f, k, r, rs, i, s)                                       \
  r = f(s + i);                                                                \
  if (r.error().empty()) {                                                     \
    rs.push(MapRecord<std::string, Json>(k, *r.value()->key));                 \
    delete r.value()->key;                                                     \
    i += r.value()->value;                                                     \
    continue;                                                                  \
  }
#endif
