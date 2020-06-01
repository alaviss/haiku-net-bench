#ifndef OVERHEAD_COMMON_H
#define OVERHEAD_COMMON_H

#define panic(expr, msg, onPanic) \
  { \
    if (expr) { \
      perror(msg); \
      onPanic; \
    } \
  }

#ifndef NO_CONNECTIONS
#define NO_CONNECTIONS 100
#endif
#define CHUNK 4096 * 1024
#define REQUEST "GET /testpayload HTTP/1.0\r\n" "Host: localhost\r\n" \
  "Accept: */*\r\n" "User-Agent: overhead/0.0.1\r\n" "\r\n"
#define TEST_ADDRESS htonl(INADDR_LOOPBACK)
#define TEST_PORT htons(8000)
#define MiB (1024 * 1024)
#define SEC 1000000000.0
#define USEC 1000.0

#define EVENT_RW (POLLIN | POLLOUT)
#define EVENT_INVALID (POLLERR | POLLHUP | POLLNVAL)

#endif
