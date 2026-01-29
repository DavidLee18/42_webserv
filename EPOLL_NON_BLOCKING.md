# EPoll and Non-Blocking Sockets: Why Both Are Required

## Executive Summary

**Yes, sockets MUST be set to non-blocking mode when using EPoll with edge-triggered (ET) mode.**

The `FileDescriptor::set_nonblocking()` method is **essential** and cannot be removed. This document explains why.

## Understanding Edge-Triggered Mode

### Edge-Triggered (ET) vs Level-Triggered (LT)

- **Level-Triggered (default)**: EPoll notifies you repeatedly as long as the condition is true
  - Example: If data is available, EPoll keeps notifying until you read all the data
  - Simpler to use but less efficient due to repeated notifications

- **Edge-Triggered (EPOLLET)**: EPoll notifies you only once when the state changes
  - Example: EPoll notifies once when data arrives, then won't notify again until NEW data arrives
  - More efficient but requires careful handling
  - **This is what our code uses** (see `Option opt_flags(true, ...)` in main.cpp)

## Why Non-Blocking Mode is Required

### The Problem

When using edge-triggered mode, EPoll only notifies **once** per state change. This means:

1. When a connection arrives on a listening socket, EPoll notifies once
2. If multiple connections arrive before you call `accept()`, you only get ONE notification
3. You MUST call `accept()` in a loop until it returns EAGAIN to get all pending connections

The same applies to reading data:
1. When data arrives, EPoll notifies once
2. If more data arrives before you read, you only get ONE notification
3. You MUST call `recv()` in a loop until it returns EAGAIN to read all available data

### What Happens WITHOUT Non-Blocking Mode

```cpp
// WRONG: This will FREEZE your server!
while (true) {
    // If socket is blocking and no more connections pending:
    // This accept() will BLOCK FOREVER, freezing the entire event loop
    int client = accept(listen_sock, ...);  
}
```

**Result**: Your entire server hangs. No other clients can be served. The event loop is stuck.

### What Happens WITH Non-Blocking Mode

```cpp
// CORRECT: Server stays responsive
while (true) {
    int client = accept(listen_sock, ...);
    if (client < 0 && errno == EAGAIN) {
        // No more connections pending, exit loop gracefully
        break;  
    }
    // Process client...
}
```

**Result**: The loop exits cleanly when no more data is available. The event loop continues processing other file descriptors.

## Implementation in This Codebase

### 1. Setting Non-Blocking Mode

**Listening sockets** (main.cpp:92-96):
```cpp
Result<Void> rnonblock = sock.set_nonblocking();
```

**Client sockets** (main.cpp:180-184):
```cpp
Result<Void> rnonblock = client_fd.set_nonblocking();
```

**Implementation** (file_descriptor.cpp:187-196):
```cpp
Result<Void> FileDescriptor::set_nonblocking() {
  int flags = fcntl(_fd, F_GETFL, 0);
  if (flags < 0) {
    return ERR(Void, "Failed to get file descriptor flags");
  }
  if (fcntl(_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    return ERR(Void, "Failed to set non-blocking mode");
  }
  return OKV;
}
```

### 2. Using Edge-Triggered Mode

**EPoll configuration** (main.cpp:129, 192):
```cpp
Option opt_flags(true, false, false, false);  // et=true enables edge-triggered
```

### 3. Draining in Loops

**Accepting connections** (main.cpp:169-203):
```cpp
// Accept all pending connections (edge-triggered mode)
while (true) {
    Result<FileDescriptor> rclient = listen_sockets[i]->socket_accept(...);
    
    if (!rclient.error().empty()) {
        if (rclient.error() != Errors::try_again) {
            std::cerr << "Accept failed: " << rclient.error() << std::endl;
        }
        break;  // EAGAIN - no more connections
    }
    // Handle connection...
}
```

**Reading data** (main.cpp:225-246):
```cpp
// Read all available data (edge-triggered mode)
while (true) {
    char buffer[4096];
    Result<ssize_t> rread = client_fdptr->sock_recv(buffer, sizeof(buffer) - 1);
    
    if (!rread.error().empty()) {
        if (rread.error() != Errors::try_again) {
            std::cerr << "Read error: " << rread.error() << std::endl;
            read_error = true;
        }
        break;  // EAGAIN - no more data
    }
    // Process data...
}
```

## The Complete Pattern

The **edge-triggered + non-blocking I/O** pattern is the standard approach for scalable event-driven servers. Here's the full pattern:

```
1. Create socket
2. Set socket to NON-BLOCKING mode ← CRITICAL
3. Add socket to EPoll with EDGE-TRIGGERED flag
4. In event loop:
   a. Wait for EPoll events
   b. When event occurs, process in a LOOP
   c. Continue until operation returns EAGAIN/EWOULDBLOCK
   d. Return to waiting for next event
```

## What Would Break If We Removed set_nonblocking()

| Component | Impact |
|-----------|--------|
| **Listening socket** | First `accept()` call might block indefinitely → Server hangs, can't accept new connections |
| **Client socket reads** | First `recv()` call might block indefinitely → Event loop freezes, other clients can't be served |
| **Event loop** | Blocked operation prevents processing other file descriptors → Server becomes unresponsive |
| **Edge-triggered semantics** | Missed events won't retrigger → Data loss, connections lost |

## Alternatives (None Viable)

### Could we use level-triggered mode instead?
- **Yes**, level-triggered (LT) mode works with blocking sockets
- **But** LT is less efficient: repeated notifications for the same data
- **Our code is designed for ET**: loops expect EAGAIN, not infinite blocking

### Could we use blocking mode with timeouts?
- **No**: Timeouts don't prevent blocking, they just limit how long
- **Still freezes** the event loop during timeout period
- **Defeats the purpose** of event-driven architecture

### Could we use separate threads?
- **Different architecture**: Would require complete rewrite
- **Not the question**: The question is about EPoll + non-blocking pattern
- **Still needs non-blocking**: Even with threads, ET mode requires non-blocking sockets

## Conclusion

The `FileDescriptor::set_nonblocking()` method is **absolutely essential** and **cannot be removed**. 

It is a fundamental requirement of the edge-triggered EPoll pattern that this codebase uses. Without it, the server would hang on the first blocking operation, making it completely non-functional.

The documentation has been updated throughout the codebase to explain this requirement:
- `file_descriptor.h`: Comprehensive documentation on `set_nonblocking()`
- `epoll_kqueue.h`: Documentation on `Option` and `EPoll` classes
- `main.cpp`: Inline comments explaining the pattern at key points

## References

- [epoll(7) man page](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [fcntl(2) man page - O_NONBLOCK](https://man7.org/linux/man-pages/man2/fcntl.2.html)
- "The Linux Programming Interface" by Michael Kerrisk, Chapter 63: Alternative I/O Models
