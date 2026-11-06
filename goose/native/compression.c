/* System-zlib binding. Frame limits, thresholds and packet validation are Goose. */
#include "compression.h"
#include <limits.h>
#include <stdlib.h>
#include <zlib.h>

int64_t gx_zlib_bound(int64_t length) {
    if (length < 0 || (uint64_t)length > UINT_MAX) return -1;
    return (int64_t)compressBound((uLong)length);
}
int32_t gx_deflate(gx_const_bytes input, gx_bytes output, int64_t *written) {
    *written = 0;
    if (input.len < 0 || output.len < 0 || (uint64_t)input.len > UINT_MAX ||
        (uint64_t)output.len > UINT_MAX) return Z_STREAM_ERROR;
    uLongf size = (uLongf)output.len;
    int status = compress2(output.data, &size, input.data, (uLong)input.len, Z_DEFAULT_COMPRESSION);
    *written = (int64_t)size;
    return status;
}
uint64_t gx_inflater_open(void) {
    z_stream *stream = calloc(1, sizeof(*stream));
    if (!stream) return 0;
    if (inflateInit(stream) != Z_OK) { free(stream); return 0; }
    return (uint64_t)(uintptr_t)stream;
}
void gx_inflater_close(uint64_t handle) {
    if (!handle) return;
    z_stream *stream = (z_stream *)(uintptr_t)handle;
    inflateEnd(stream);
    free(stream);
}
int32_t gx_inflate(uint64_t handle, gx_const_bytes input, gx_bytes output, int64_t *written) {
    *written = 0;
    if (!handle || input.len < 0 || output.len < 0 || (uint64_t)input.len > UINT_MAX ||
        (uint64_t)output.len > UINT_MAX) return Z_STREAM_ERROR;
    z_stream *stream = (z_stream *)(uintptr_t)handle;
    int status = inflateReset(stream);
    if (status != Z_OK) return status;
    stream->next_in = (Bytef *)(uintptr_t)input.data;
    stream->avail_in = (uInt)input.len;
    stream->next_out = output.data;
    stream->avail_out = (uInt)output.len;
    status = inflate(stream, Z_FINISH);
    *written = (int64_t)stream->total_out;
    /* Do not retain any borrowed Goose storage after the call. */
    stream->next_in = NULL;
    stream->avail_in = 0;
    stream->next_out = NULL;
    stream->avail_out = 0;
    return status;
}
