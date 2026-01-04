#include "webserv.h"

const std::string Errors::invalid_fd = "Invalid file descriptor";
const std::string Errors::iter_ended = "The Iterator reached end";
const std::string Errors::interrupted = "Operation interrupted";
const std::string Errors::fd_not_registered = "File descriptor not registered";
const std::string Errors::invalid_operation = "Invalid operation";
const std::string Errors::epoll_loop = "EPoll loop detected";
const std::string Errors::out_of_mem = "Out of memory";
const std::string Errors::epoll_full = "EPoll queue is full";
const std::string Errors::not_supported = "Operation not supported";
const std::string Errors::access_denied = "Access denied";
const std::string Errors::fd_too_many =
    "File descriptors are too many, reached to a limit";
const std::string Errors::addr_not_available = "Address not available";
const std::string Errors::address_fault = "Address memory fault";
const std::string Errors::addr_loop =
    "Address is behind too many symbolic links";
const std::string Errors::name_too_long = "Name too long";
const std::string Errors::not_found = "Not found";
const std::string Errors::readonly_filesys =
    "The operation is called to execute on a read-only filesystem, but is "
    "required to write.";
const std::string Errors::try_again = "Try again next time.";
const std::string Errors::conn_aborted = "A connection has been aborted.";
const std::string Errors::unknown_char = "Unknown character";
const std::string Errors::invalid_format = "Invalid Format";
const std::string Errors::too_long_num = "Too long numbers";
const std::string Errors::out_of_rng = "value out of range";
const std::string Errors::invalid_json =
    "String is not of a valid JSON format.";
