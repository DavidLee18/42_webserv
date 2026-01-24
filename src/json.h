#ifndef JSON_H
#define JSON_H

#include "result.h"
#include <string>
#include <utility>
#include <vector>

class Json;

class Json {
public:
  enum Type { Null, Bool, Num, Str, Arr, Obj };
  union Value {
    void *_null;
    bool _bool;
    long double num;
    std::string *_str;
    std::vector<Json> *arr;
    std::vector<std::pair<std::string, Json> > *obj;
  };
  static Json *null();
  static Json *_bool(bool);
  static Json *num(long double);
  static Json *str(std::string);
  static Json *arr(std::vector<Json>);
  static Json *obj(std::vector<std::pair<std::string, Json> >);

  Json() : _type(Null), _value((Value){._null = NULL}) {}
  Json(Type ty, Value val) : _type(ty), _value(val) {}
  Json(const Json &other);
  ~Json() {
    switch (_type) {
    case Str:
      delete _value._str;
      break;
    case Arr:
      delete _value.arr;
      break;
    case Obj:
      delete _value.obj;
      break;
    default:
      break;
    }
  }
  const Type &type() const { return _type; }
  const Value &value() const { return _value; }
  friend std::ostream &operator<<(std::ostream &os, Json &js);

  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<Json *, size_t> > null_or_undef(const char *);
    static Result<std::pair<Json *, size_t> > _boolean(const char *);
    static Result<std::pair<Json *, size_t> > _num(const char *, char end);
    static Result<std::pair<Json *, size_t> > _str(const char *);
    static Result<std::pair<Json *, size_t> > _arr(const char *);
    static Result<std::pair<Json *, size_t> > _obj(const char *);

  public:
    static Result<std::pair<Json *, size_t> > parse(const char *, char);
  };

private:
  Type _type;
  Value _value;
};

std::ostream &operator<<(std::ostream &, std::vector<Json> &);
std::ostream &operator<<(std::ostream &, std::pair<std::string, Json> &);

#define TRY_PARSE(f, r, rs, i, s)                                              \
  Result<std::pair<Json *, size_t> > r = f(s + i);                             \
  if (r.error().empty()) {                                                     \
    rs.push_back(*r.value()->first);                                           \
    delete r.value()->first;                                                   \
    i += r.value()->second;                                                    \
    continue;                                                                  \
  }

#define TRY_PARSE_NUM(r, rs, i, s, ed)                                         \
  Result<std::pair<Json *, size_t> > r = _num(s + i, ed);                      \
  if (r.error().empty()) {                                                     \
    rs.push_back(*r.value()->first);                                           \
    delete r.value()->first;                                                   \
    i += r.value()->second;                                                    \
    continue;                                                                  \
  }

#define TRY_PARSE_PAIR(f, k, r, rs, i, s)                                      \
  Result<std::pair<Json *, size_t> > r = f(s + i);                             \
  if (r.error().empty()) {                                                     \
    rs.push_back(make_pair(k, *r.value()->first));                             \
    delete r.value()->first;                                                   \
    i += r.value()->second;                                                    \
    continue;                                                                  \
  }
#define TRY_PARSE_PAIR_NUM(k, r, rs, i, s, ed)                                 \
  Result<std::pair<Json *, size_t> > r = _num(s + i, ed);                      \
  if (r.error().empty()) {                                                     \
    rs.push_back(std::pair<std::string, Json>(k, *r.value()->first));          \
    delete r.value()->first;                                                   \
    i += r.value()->second;                                                    \
    continue;                                                                  \
  }

#endif
