#ifndef RESULT_H
#define RESULT_H

#include <cstddef>
#include <iostream>
#include <string>
#include <cassert>
#include <cstring>

// Optional wrapper for C++98 - holds a value or nothing
template <typename T>
class Optional {
  bool _has_value;
  // Use max_align_t equivalent for C++98 to ensure proper alignment
  // Use a properly aligned storage buffer
  union AlignedStorage {
    char _dummy;
    long _align1;
    double _align2;
    long double _align3;
    void* _align4;
  };
  char _storage[sizeof(T) > sizeof(AlignedStorage) ? sizeof(T) : sizeof(AlignedStorage)];

  T* ptr() { return reinterpret_cast<T*>(_storage); }
  const T* ptr() const { return reinterpret_cast<const T*>(_storage); }

public:
  Optional() : _has_value(false) {
    // Storage is uninitialized, will be constructed via placement new if needed
  }
  
  Optional(const T& val) : _has_value(true) {
    new (ptr()) T(val);
  }
  
  Optional(const Optional& other) : _has_value(other._has_value) {
    if (_has_value) {
      new (ptr()) T(*other.ptr());
    }
    // If not has_value, storage remains uninitialized
  }
  
  ~Optional() {
    if (_has_value) {
      ptr()->~T();
    }
  }
  
  Optional& operator=(const Optional& other) {
    if (this != &other) {
      if (_has_value) {
        ptr()->~T();
      }
      _has_value = other._has_value;
      if (_has_value) {
        new (ptr()) T(*other.ptr());
      }
      // If not has_value, storage remains uninitialized
    }
    return *this;
  }
  
  bool has_value() const { return _has_value; }
  
  const T& get() const {
    assert(_has_value);
    return *ptr();
  }
  
  T& get() {
    assert(_has_value);
    return *ptr();
  }
};

// Result type that holds either a value or an error
template <typename T> class Result {
  Optional<T> _val;
  std::string _err;

public:
  // Constructor for success case
  Result(const T &v, const std::string &e) : _val(v), _err(e) {}
  
  // Constructor for error case
  explicit Result(const std::string &e) : _val(), _err(e) {}
  
  // Copy constructor
  Result(const Result& other) : _val(other._val), _err(other._err) {}
  
  // Assignment operator
  Result& operator=(const Result& other) {
    if (this != &other) {
      _val = other._val;
      _err = other._err;
    }
    return *this;
  }
  
  bool has_value() const { return _val.has_value(); }
  
  const T &value() const { 
    assert(_val.has_value() && "Attempted to access value of an error Result");
    return _val.get(); 
  }
  
  T &value_mut() { 
    assert(_val.has_value() && "Attempted to access value of an error Result");
    return _val.get(); 
  }
  
  const std::string &error() const { return _err; }
};

#define OK(t, v) (Result<t>(v, ""))

#define ERR(t, e) (Result<t>(e))

#define TRY(t, vt, v, r)                                                       \
  if (!(r).error().empty()) {                                                  \
    return ERR(t, (r).error());                                                \
  } else {                                                                     \
    v = (r).value();                                                           \
  }

#define TRYF(t, vt, v, r, f)                                                   \
  if (!(r).error().empty()) {                                                  \
    f return ERR(t, (r).error());                                              \
  } else {                                                                     \
    v = (r).value();                                                           \
  }

struct Void {};

#define VOID Void()

#define OKV OK(Void, VOID)

#define PANIC(e)                                                               \
  if (!(e).error().empty()) {                                                  \
    std::cerr << (e).error() << std::endl;                                     \
    return 1;                                                                  \
  }

#define OK_PAIR(t1, t2, v1, v2)                                                \
  Result<std::pair<t1, t2> >(std::make_pair(v1, v2), "")

#define ERR_PAIR(t1, t2, e) Result<std::pair<t1, t2> >(e)

#define TRY_PAIR(t1, t2, v, r)                                                 \
  if ((r).error().empty()) {                                                   \
    v = (r).value();                                                           \
  } else {                                                                     \
    return (r);                                                                \
  }

#endif // RESULT_H
