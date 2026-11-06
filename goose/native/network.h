#ifndef XYLAR_GOOSE_NETWORK_H
#define XYLAR_GOOSE_NETWORK_H
#include "compression.h"
/* POSIX transport only. Framing, buffering, counters and protocol stay in Goose. */
int32_t gx_tcp_connect(gx_const_bytes host, uint16_t port, int64_t timeout_ms, int64_t *fd, int32_t *error);
int64_t gx_network_message(int32_t code, int32_t lookup, gx_bytes output);
int32_t gx_network_kind(int32_t code);
int64_t gx_tcp_read(int64_t fd, gx_bytes output, int64_t timeout_ms);
int32_t gx_tcp_write(int64_t fd, gx_const_bytes input);
void gx_tcp_close(int64_t fd);
#endif
