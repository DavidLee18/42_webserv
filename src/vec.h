#ifndef VEC_H
#define VEC_H
#include "result.h"
#include <cstddef>

template <typename T> class Vec {
  T *_data;
  size_t _size;
  size_t _cap;

  void alloc() {
    const size_t new_capacity = _cap == 0 ? 1 : _cap * 2;
    T *new_data = static_cast<T *>(operator new(new_capacity * sizeof(T)));
    if (_data != NULL) {
      for (size_t i = 0; i < _size; ++i) {
        new (new_data + i) T(_data[i]);
        _data[i].~T();
      }
      operator delete(_data);
    }
    _data = new_data;
    _cap = new_capacity;
  }

public:
  Vec() : _data(NULL), _size(0), _cap(0) {}

  explicit Vec(const size_t capacity)
      : _data(operator new(capacity * sizeof(T))), _size(0), _cap(capacity) {}

  ~Vec() {
    for (size_t i = 0; i < _size; ++i) {
      _data[i].~T();
    }
    operator delete(_data);
  }

  void push(T value) {
    if (_size >= _cap)
      alloc();
    new (_data + _size++) T(value);
  }

  Result<T> pop() {
    if (_size == 0)
      return ERR(T, "Cannot pop from empty vector");
    T *value = new T(_data[--_size]);
    _data[_size].~T();
    return OK(T, value);
  }

  template <typename U> Result<const T *> find(const U &other) const {
    for (size_t i = 0; i < _size; ++i) {
      if (_data[i] == other) {
        T *const *t = new const T *;
        *t = _data + i;
        return OK(T, t);
      }
    }
    return ERR(T, "not found");
  }

  size_t size() const { return _size; }
  size_t cap() const { return _cap; }
  bool empty() const { return _size == 0; }

  const Result<T *> get(size_t index) const {
    if (index >= _size) {
      return ERR(T, "Out of index");
    }
    T **res = new T *;
    *res = _data + index;
    OK(T, res);
  }
};

#endif // VEC_H
