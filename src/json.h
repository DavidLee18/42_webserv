#ifndef JSON_H
#define JSON_H

#include "map_record.h"
#include "result.h"
#include <string>

struct Json;

struct Jsons {
  Json *ptr;
  size_t size;
};

struct chars {
  char *ptr;
  size_t size;
};

struct Map {
  MapRecord<std::string, Json> *ptr;
  size_t size;
};

struct Json {
  enum { JsonNull, JsonBool, JsonNum, JsonStr, JsonArr, JsonObj } type;
  union {
    void *_null;
    bool _bool;
    long double num;
    chars _str;
    Jsons arr;
    Map obj;
  } value;
  static Result<Json> from_raw(const char *raw);

  static Json *null();
  static Json *_bool(bool);
  static Json *num(long double);
  static Json *str(std::string);
  static Json *arr(Jsons);
  static Json *obj(Map);

  class Parser {
    virtual void phantom() = 0;
    public:
    static Result<MapRecord<Json *, size_t> > null_or_undef(const char *);
    static Result<MapRecord<Json *, size_t> > _boolean(const char *);
    static Result<MapRecord<Json *, size_t> > _num(const char *);
    static Result<MapRecord<Json *, size_t> > _str(const char *);
    static Result<MapRecord<Json *, size_t> > _arr(const char *);
    static Result<MapRecord<Json *, size_t> > _obj(const char *);
  };
};

#define TRY_PARSE(f, r, rs, i, s)                                              \
  r = f(s + i);                                                                \
  if (r.err == NULL) {                                                         \
    rs.push(*r.val->key);                                                      \
    delete r.val->key;                                                         \
    i += r.val->value;                                                         \
    continue;                                                                  \
  }

#define TRY_PARSE_REC(f, k, r, rs, i, s)                                       \
  r = f(s + i);                                                                \
  if (r.err == NULL) {                                                         \
    rs.push(MapRecord<std::string, Json>(k, *r.val->key));                     \
    delete r.val->key;                                                         \
    i += r.val->value;                                                         \
    continue;                                                                  \
  }
#endif
