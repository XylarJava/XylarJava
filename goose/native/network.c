/* Native adaptation of TcpStream/resolver scheduling to POSIX sockets. */
#define _POSIX_C_SOURCE 200809L
#include "network.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
static int64_t now_ms(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (int64_t)time.tv_sec * 1000 + time.tv_nsec / 1000000;
}
static int wait_fd(int fd, short events, int64_t timeout) {
    struct pollfd p = {fd, events, 0};
    int n = poll(&p, 1, timeout < 0 ? -1 : timeout > INT_MAX ? INT_MAX : (int)timeout);
    if (n < 0) return errno;
    return n == 0 ? ETIMEDOUT : 0;
}
int32_t gx_tcp_connect(gx_const_bytes host, uint16_t port, int64_t timeout_ms, int64_t *fd, int32_t *error) {
    *fd = -1;
    *error = 0;
    if (timeout_ms <= 0) { *error = EINVAL; return 2; }
    if (host.len <= 0 || host.len > 65535 || memchr(host.data, 0, (size_t)host.len)) {
        *error = EAI_NONAME;
        return 1;
    }
    char *name = malloc((size_t)host.len + 1);
    if (!name) { *error = ENOMEM; return 2; }
    memcpy(name, host.data, (size_t)host.len);
    name[host.len] = 0;
    char service[6];
    snprintf(service, sizeof(service), "%u", (unsigned)port);
    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int code = getaddrinfo(name, service, &hints, &addresses);
    free(name);
    if (code) { *error = code; return 1; }
    size_t count = 0;
    for (struct addrinfo *a = addresses; a; a = a->ai_next) ++count;
    struct addrinfo **order = calloc(count, sizeof(*order));
    struct pollfd *polls = calloc(count, sizeof(*polls));
    int64_t *deadlines = calloc(count, sizeof(*deadlines));
    if (!order || !polls || !deadlines) {
        free(order); free(polls); free(deadlines); freeaddrinfo(addresses);
        *error = ENOMEM; return 2;
    }
    size_t cursor = 0;
    for (struct addrinfo *a = addresses; a; a = a->ai_next) if (a->ai_family != AF_INET6) order[cursor++] = a;
    for (struct addrinfo *a = addresses; a; a = a->ai_next) if (a->ai_family == AF_INET6) order[cursor++] = a;
    for (size_t i = 0; i < count; ++i) polls[i].fd = -1;
    size_t started = 0, pending = 0;
    int first_error = 0, start_next = 1;
    int64_t next_start = 0;
    while (started < count || pending) {
        if (started < count && (start_next || now_ms() >= next_start)) {
            size_t i = started++;
            struct addrinfo *a = order[i];
            int s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
            int err = s < 0 ? errno : 0;
            if (s >= 0) {
                if (fcntl(s, F_SETFD, FD_CLOEXEC) < 0 || fcntl(s, F_SETFL, O_NONBLOCK) < 0) err = errno;
                int yes = 1;
                if (!err && setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0) err = errno;
                if (!err && connect(s, a->ai_addr, a->ai_addrlen) == 0) { *fd = s; goto finished; }
                if (!err) err = errno;
                if (err == EINPROGRESS) {
                    polls[i] = (struct pollfd){s, POLLOUT, 0};
                    deadlines[i] = now_ms() + (timeout_ms < 0 ? INT_MAX : timeout_ms);
                    ++pending;
                } else { close(s); }
            }
            if (err != EINPROGRESS && i == 0) first_error = err;
            start_next = err != EINPROGRESS;
            next_start = now_ms() + 250;
            if (start_next || !pending) continue;
        }
        int64_t now = now_ms(), until = INT_MAX;
        if (started < count) until = next_start - now;
        for (size_t i = 0; i < started; ++i) if (polls[i].fd >= 0 && deadlines[i] - now < until) until = deadlines[i] - now;
        int ready = poll(polls, started, until <= 0 ? 0 : until > INT_MAX ? INT_MAX : (int)until);
        if (ready < 0 && errno != EINTR) {
            if (!first_error) first_error = errno;
            break;
        }
        now = now_ms();
        for (size_t i = 0; i < started; ++i) if (polls[i].fd >= 0) {
            int err = 0;
            if (polls[i].revents) {
                socklen_t size = sizeof(err);
                if (getsockopt(polls[i].fd, SOL_SOCKET, SO_ERROR, &err, &size) < 0) err = errno;
                if (!err) { *fd = polls[i].fd; polls[i].fd = -1; goto finished; }
            } else if (now >= deadlines[i]) err = ETIMEDOUT;
            else continue;
            if (i == 0) first_error = err;
            close(polls[i].fd); polls[i].fd = -1; --pending;
            start_next = 1;
        }
    }
finished:
    for (size_t i = 0; i < started; ++i) if (polls[i].fd >= 0) close(polls[i].fd);
    free(order); free(polls); free(deadlines); freeaddrinfo(addresses);
    if (*fd >= 0) return 0;
    *error = first_error ? first_error : EHOSTUNREACH;
    return 2;
}
int64_t gx_network_message(int32_t code, int32_t lookup, gx_bytes output) {
    const char *message = lookup ? gai_strerror(code) : strerror(code);
    size_t length = strlen(message);
    if (output.len < 0) return 0;
    if (length > (size_t)output.len) length = (size_t)output.len;
    memcpy(output.data, message, length);
    return (int64_t)length;
}
int32_t gx_network_kind(int32_t code) {
    if (code == EINTR) return 1;
    if (code == ETIMEDOUT || code == EAGAIN || code == EWOULDBLOCK) return 2;
    return 0;
}
int64_t gx_tcp_read(int64_t fd, gx_bytes output, int64_t timeout_ms) {
    if (fd < 0 || fd > INT_MAX || output.len < 0) return -ENOTCONN;
    int error = wait_fd((int)fd, POLLIN, timeout_ms);
    if (error) return -error;
    ssize_t n = recv((int)fd, output.data, (size_t)output.len, 0);
    return n < 0 ? -errno : n;
}
int32_t gx_tcp_write(int64_t fd, gx_const_bytes input) {
    if (fd < 0 || fd > INT_MAX || input.len < 0) return ENOTCONN;
    size_t sent = 0;
    while (sent < (size_t)input.len) {
        ssize_t n = send((int)fd, input.data + sent, (size_t)input.len - sent, MSG_NOSIGNAL);
        if (n > 0) { sent += (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            int error = wait_fd((int)fd, POLLOUT, -1);
            if (error == EINTR) continue;
            if (error) return error;
            continue;
        }
        return n < 0 ? errno : EPIPE;
    }
    return 0;
}
void gx_tcp_close(int64_t fd) {
    if (fd >= 0 && fd <= INT_MAX) { shutdown((int)fd, SHUT_RDWR); close((int)fd); }
}
