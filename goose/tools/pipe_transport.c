/* Test-only adapter: a subprocess's stdin/stdout stand in for the byte stream.
 * The production target links native/network.c instead. No sockets are used. */
#define _POSIX_C_SOURCE 200809L
#include "../native/network.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int32_t gx_tcp_connect(gx_const_bytes host, uint16_t port, int64_t timeout, int64_t *fd, int32_t *error) {
    (void)host; (void)port; (void)timeout; *fd = -1; *error = ENOTSUP; return 2;
}
int64_t gx_network_message(int32_t code, int32_t lookup, gx_bytes output) {
    (void)lookup;
    const char *message = strerror(code);
    size_t n = strlen(message);
    if (output.len < 0) return 0;
    if (n > (size_t)output.len) n = (size_t)output.len;
    memcpy(output.data, message, n);
    return (int64_t)n;
}
int32_t gx_network_kind(int32_t code) {
    if (code == EINTR) return 1;
    return code == ETIMEDOUT || code == EAGAIN || code == EWOULDBLOCK ? 2 : 0;
}
int64_t gx_tcp_read(int64_t fd, gx_bytes output, int64_t timeout) {
    (void)fd;
    struct pollfd p = {STDIN_FILENO, POLLIN, 0};
    int n = poll(&p, 1, timeout < 0 ? -1 : (int)timeout);
    if (n < 0) return -errno;
    if (!n) return -ETIMEDOUT;
    const char *limit = getenv("XYLAR_TEST_READ_LIMIT");
    size_t size = (size_t)output.len;
    if (limit) {
        unsigned long cap = strtoul(limit, NULL, 10);
        if (cap && cap < size) size = cap;
    }
    ssize_t read_bytes = read(STDIN_FILENO, output.data, size);
    return read_bytes < 0 ? -errno : read_bytes;
}
int32_t gx_tcp_write(int64_t fd, gx_const_bytes input) {
    (void)fd;
    size_t at = 0;
    while (at < (size_t)input.len) {
        ssize_t n = write(STDOUT_FILENO, input.data + at, (size_t)input.len - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return n < 0 ? errno : EPIPE;
        at += (size_t)n;
    }
    return 0;
}
void gx_tcp_close(int64_t fd) { (void)fd; }
