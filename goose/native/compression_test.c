/* Bounds, stream reset, independent owners and cleanup of the zlib binding. */
#include "compression.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#define CHECK(condition) do { if (!(condition)) abort(); } while (0)
int main(void) {
    const int64_t length = 70000;
    uint8_t *input = malloc((size_t)length);
    int64_t capacity = gx_zlib_bound(length);
    uint8_t *compressed = malloc((size_t)capacity);
    uint8_t *output = malloc((size_t)length + 1);
    CHECK(input && compressed && output);
    for (int64_t i = 0; i < length; ++i) input[i] = (uint8_t)(i % 13);
    int64_t size = 0;
    CHECK(gx_deflate((gx_const_bytes){input, length}, (gx_bytes){compressed, capacity}, &size) == Z_OK);
    CHECK(size > 4 && size < length);
    for (int n = 0; n < 32; ++n) {
        uint64_t a = gx_inflater_open(), b = gx_inflater_open();
        CHECK(a && b && a != b);
        for (int i = 0; i < 2; ++i) {
            int64_t written = 0;
            compressed[size - 1] ^= 0xff;
            CHECK(gx_inflate(a, (gx_const_bytes){compressed, size}, (gx_bytes){output, length + 1}, &written) == Z_DATA_ERROR);
            compressed[size - 1] ^= 0xff;
            CHECK(gx_inflate(b, (gx_const_bytes){compressed, size}, (gx_bytes){output, length + 1}, &written) == Z_STREAM_END);
            CHECK(written == length && memcmp(input, output, (size_t)length) == 0);
            CHECK(gx_inflate(a, (gx_const_bytes){compressed, size}, (gx_bytes){output, length - 1}, &written) == Z_BUF_ERROR);
            CHECK(written == length - 1);
            CHECK(gx_inflate(a, (gx_const_bytes){compressed, size}, (gx_bytes){output, length + 1}, &written) == Z_STREAM_END);
            CHECK(written == length && memcmp(input, output, (size_t)length) == 0);
            CHECK(gx_inflate(a, (gx_const_bytes){compressed, size - 1}, (gx_bytes){output, length + 1}, &written) == Z_BUF_ERROR);
        }
        gx_inflater_close(a);
        gx_inflater_close(b);
    }
    gx_inflater_close(0);
    int64_t written = 123;
    CHECK(gx_inflate(0, (gx_const_bytes){compressed, size}, (gx_bytes){output, length + 1}, &written) == Z_STREAM_ERROR && written == 0);
    CHECK(gx_deflate((gx_const_bytes){input, length}, (gx_bytes){output, 1}, &written) == Z_BUF_ERROR);
    free(output);
    free(compressed);
    free(input);
    return 0;
}
