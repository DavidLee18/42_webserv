//
// Created by 이재현 on 2025-11-12.
//

#ifndef INC_42_WEBSERV_VEC_H
#define INC_42_WEBSERV_VEC_H
#include <cstddef>
#include <stdexcept>

template<typename T>
class Vec {
    T *_data;
    size_t _size;
    size_t _cap;

public:
    Vec() : _data(NULL), _size(0), _cap(0) {}

    explicit Vec(const size_t capacity) : _data(operator new (capacity * sizeof(T))), _size(0), _cap(capacity) {}

    ~Vec() {
        for (size_t i = 0; i < _size; ++i) {
            _data[i].~T();
        }
        operator delete(_data);
    }

    void alloc() {
        const size_t new_capacity = _cap == 0 ? 1 : _cap * 2;
        T* new_data = static_cast<T *>(operator new(new_capacity * sizeof(T)));
        for (size_t i = 0; i < _size; ++i) {
            new (new_data + i) T(_data[i]);
            _data[i].~T();
        }
        operator delete(_data);
        _data = new_data;
        _cap = new_capacity;
    }

    void push(T value) {
        if (_size >= _cap)
            alloc();
        new (_data + _size++) T(value);
    }

    T pop() throw(std::out_of_range) {
        if (_size == 0)
            throw std::out_of_range("Cannot pop from empty vector");
        T value(_data[--_size]);
        _data[_size].~T();
        return value;
    }

    template<typename U>
    bool elem(const U& other) const {
        for (size_t i = 0; i < _size; ++i) {
            if (_data[i] == other)
                return true;
        }
        return false;
    }

    size_t size() const { return _size; }
    size_t cap() const { return _cap; }
    bool empty() const { return _size == 0; }

    const T& operator[](size_t index) const { return _data[index]; }
};

#endif //INC_42_WEBSERV_VEC_H