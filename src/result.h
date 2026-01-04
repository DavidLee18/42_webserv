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
  const T *const value() { return val; }
  const std::string &error() { return err; }

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

#endif // RESULT_H
