#ifndef RESULT_H
#define RESULT_H

#include <cstddef>
#include <iostream>
#include <string>

template <typename T> class Result {
  T *val;
  std::string err;

public:
  Result(T *v, std::string e) : val(v), err(e) {}
  const T *value() const { return val; }
  const std::string &error() const { return err; }

  ~Result() {
    if (val != NULL)
      delete val;
  }
};

#define OK(t, v) (Result<t>(v, ""))

#define ERR(t, e) (Result<t>(NULL, e))

#define TRY(t, vt, v, r)                                                       \
  if (!(r).error().empty()) {                                                  \
    return Result<t>(NULL, (r).error());                                       \
  } else {                                                                     \
    v = const_cast<vt *>((r).value());                                         \
  }

#define TRYF(t, vt, v, r, f)                                                   \
  if (!(r).error().empty()) {                                                  \
    f return Result<t>(NULL, (r).error());                                     \
  } else {                                                                     \
    v = const_cast<vt *>((r).value());                                         \
  }

struct Void {};

#define VOID new Void

#define OKV OK(Void, VOID)

#define PANIC(e)                                                               \
  if (!(e).error().empty()) {                                                  \
    std::cerr << (e).error() << std::endl;                                     \
    return 1;                                                                  \
  }

#define OK_PAIR(t1, t2, v1, v2)                                                \
  Result<std::pair<t1, t2> >(new std::pair<t1, t2>(std::make_pair(v1, v2)), "")

#define ERR_PAIR(t1, t2, e) Result<std::pair<t1, t2> >(NULL, e)

#define TRY_PAIR(t1, t2, v, r)                                                 \
  if ((r).error().empty()) {                                                   \
    v = const_cast<std::pair<t1, t2> *>((r).value());                          \
  } else {                                                                     \
    return (r);                                                                \
  }

#endif // RESULT_H
