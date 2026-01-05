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

  Vec(const Vec<T> &other) {
    _cap = other._cap;
    _size = other._size;

    if (other._data != NULL && other._cap != 0) {
      _data = static_cast<T *>(operator new(_cap * sizeof(T)));
      for (size_t i = 0; i < _size; ++i) {
        new (_data + i) T(other._data[i]);
      }
    } else {
      _data = NULL;
    }
  }

  ~Vec() {
    if (_data != NULL) {
      for (size_t i = 0; i < _size; ++i) {
        _data[i].~T();
      }
      operator delete(_data);
      _data = NULL;
      _size = 0;
      _cap = 0;
    }
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

  template <typename U> Result<T *> find(const U &other) const {
    for (size_t i = 0; i < _size; ++i) {
      if (_data[i] == other) {
        T **t = new T *;
        *t = _data + i;
        return OK(T *, t);
      }
    }
    return ERR(T *, "not found");
  }

  size_t size() const { return _size; }
  size_t cap() const { return _cap; }
  bool empty() const { return _size == 0; }

  Result<T *> get(size_t index) const {
    if (index >= _size) {
      return ERR(T *, "Out of index");
    }
    T **res = new T *;
    *res = _data + index;
    return OK(T *, res);
  }

  friend std::ostream &operator<<(std::ostream &os, Vec<T> &vec) {
    os << '[';
    for (size_t i = 0; i < vec._size; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << vec._data[i];
    }
    os << ']';
    return os;
  }
};

#endif // VEC_H
