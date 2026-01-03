#ifndef RESULT_H
#define RESULT_H

#include <cstddef>
#include <iostream>
#include <string>

template <typename T> struct Result {
  T *val;
  std::string err;

  Result(T *v, std::string e) : val(v), err(e) {
    std::cout << "Result(" << v << ", \"" << e << "\")" << std::endl;
  }

  ~Result() {
    std::cout << "~Result(" << val << ", \"" << err << "\")" << std::endl;
    if (val != NULL)
      delete val;
  }
};

#define OK(t, v) (Result<t>(v, ""))

#define ERR(t, e) (Result<t>(NULL, e))

#define TRY(t, v, r)                                                           \
  if (!(r).err.empty()) {                                                      \
    return (Result<t>(reinterpret_cast<t *>((r).val), (r).err));               \
  } else {                                                                     \
    v = (r).val;                                                               \
  }

#define TRYF(t, v, r, f)                                                       \
  if (!(r).err.empty()) {                                                      \
    f return (Result<t>(reinterpret_cast<t *>((r).val), (r).err));             \
  } else {                                                                     \
    v = (r).val;                                                               \
  }

struct Void {};

#define VOID new Void

#define OKV OK(Void, VOID)

#define PANIC(e)                                                               \
  if (!(e).err.empty()) {                                                      \
    std::cerr << (e).err << std::endl;                                         \
    return 1;                                                                  \
  }

#endif // RESULT_H
