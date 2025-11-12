//
// Created by 이재현 on 2025-11-12.
//

#ifndef INC_42_WEBSERV_EXCEPTIONS_H
#define INC_42_WEBSERV_EXCEPTIONS_H
#include <exception>

#define EXCEPTION(name, message)                                               \
  class name : public std::exception {                                         \
  public:                                                                      \
    const char *_Nonnull what() const throw() { return message; }              \
  };

EXCEPTION(InvalidFileDescriptorException, "Invalid file descriptor")
EXCEPTION(IteratorEndedException, "Iterator reached end")
EXCEPTION(InterruptedException, "Operation interrupted")
EXCEPTION(FdNotRegisteredException, "File descriptor not registered")
EXCEPTION(InvalidOperationException, "Invalid operation")
EXCEPTION(EPollLoopException, "EPoll loop error")
EXCEPTION(OutOfMemoryException, "Out of memory")
EXCEPTION(EPollFullException, "EPoll queue is full")
EXCEPTION(NotSupportedOperationException, "Operation not supported")

#endif // INC_42_WEBSERV_EXCEPTIONS_H
