//
// Created by 이재현 on 2025-11-10.
//
#include "webserv.h"

FileDescriptor::FileDescriptor(const int raw_fd) throw(InvalidFileDescriptorException) {
    if (raw_fd < 0) throw InvalidFileDescriptorException();
    _fd = raw_fd;
}

FileDescriptor::FileDescriptor(FileDescriptor &other) throw(InvalidFileDescriptorException) : _fd(other._fd) {
    if (other._fd < 0) throw InvalidFileDescriptorException();
    other._fd = -1;
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor other) throw(InvalidFileDescriptorException) {
    if (this != &other) {
        if (_fd < 0) throw InvalidFileDescriptorException();
        _fd = other._fd;
        other._fd = -1;
    }
    return *this;
}

FileDescriptor::~FileDescriptor() {
    if (_fd >= 2)
        close(_fd);
}

const int& FileDescriptor::operator*() const {
    return _fd;
}

bool FileDescriptor::set_blocking(const bool blocking) {
    return fcntl(_fd, F_SETFL, blocking ? 0 : O_NONBLOCK) == 0;
}

bool operator==(const int& lhs, const FileDescriptor& rhs) {
    return lhs == rhs._fd;
}

bool operator!=(const int& lhs, const FileDescriptor& rhs) {
    return !(lhs == rhs);
}