#ifndef ERRORS_H
#define ERRORS_H

namespace errors {
const char *invalid_fd = "Invalid file descriptor";
const char *iter_ended = "The Iterator reached end";
const char *interrupted = "Operation interrupted";
const char *fd_not_registered = "File descriptor not registered";
const char *invalid_operation = "Invalid operation";
const char *epoll_loop = "EPoll loop detected";
const char *out_of_mem = "Out of memory";
const char *epoll_full = "EPoll queue is full";
const char *not_supported = "Operation not supported";
const char *access_denied = "Access denied";
const char *fd_too_many = "File descriptors are too many, reached to a limit";
const char *addr_not_available = "Address not available";
const char *address_fault = "Address memory fault";
const char *addr_loop = "Address is behind too many symbolic links";
const char *name_too_long = "Name too long";
const char *not_found = "Not found";
const char *readonly_filesys =
    "The operation is called to execute on a read-only filesystem, but "
    "is required to write.";
const char *try_again = "Try again next time.";
const char *conn_aborted = "A connection has been aborted.";
} // namespace errors

#endif // ERRORS_H
