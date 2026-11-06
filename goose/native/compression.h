#ifndef XYLAR_GOOSE_COMPRESSION_H
#define XYLAR_GOOSE_COMPRESSION_H
#include <stdint.h>
/* Flat slice ABI used by Goose extern functions. No packet policy lives here. */
typedef struct { const uint8_t *data; int64_t len; } gx_const_bytes;
typedef struct { uint8_t *data; int64_t len; } gx_bytes;
int64_t gx_zlib_bound(int64_t length);
int32_t gx_deflate(gx_const_bytes input, gx_bytes output, int64_t *written);
uint64_t gx_inflater_open(void);
void gx_inflater_close(uint64_t handle);
int32_t gx_inflate(uint64_t handle, gx_const_bytes input, gx_bytes output, int64_t *written);
#endif
