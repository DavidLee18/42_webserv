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

struct Void {};

static Void *void_ = new Void;

#endif // RESULT_H
