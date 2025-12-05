#ifndef RESULT_H
#define RESULT_H

#include <cstddef>

template <typename T> struct Result {
  T *val;
  const char *err;

  ~Result() {
    if (val != NULL)
      delete val;
  }
};

#define OK(t, v) ((Result<t>){.val = v, .err = NULL})

#define ERR(t, e) ((Result<t>){.val = NULL, .err = e})

#define TRY(t, v, r)                                                           \
  if ((r).err != NULL) {                                                       \
    return (Result<t>){.val = reinterpret_cast<t *>((r).val), .err = (r).err}; \
  } else {                                                                     \
    v = (r).val;                                                               \
  }

#define TRYF(t, v, r, f)                                                       \
  if ((r).err != NULL) {                                                       \
    f return (Result<t>){.val = reinterpret_cast<t *>((r).val),                \
                         .err = (r).err};                                      \
  } else {                                                                     \
    v = (r).val;                                                               \
  }

struct Void {};

#define VOID new Void

#define OKV OK(Void, VOID)

#define PANIC(e)                                                               \
  if ((e).err != NULL) {                                                       \
    std::cerr << (e).err << std::endl;                                         \
    return 1;                                                                  \
  }

#endif // RESULT_H
