// IEEE-754 bit reinterpretation for the jmath port. Goose casts between floats
// and integers convert by value; the fdlibm kernels and the parity harness need
// the raw bit pattern, so these do the memcpy-style reinterpret C provides.
#include <stdint.h>
#include <string.h>

uint64_t gx_f64_bits(double x) {
    uint64_t u;
    memcpy(&u, &x, 8);
    return u;
}

double gx_f64_from_bits(uint64_t u) {
    double x;
    memcpy(&x, &u, 8);
    return x;
}

uint32_t gx_f32_bits(float x) {
    uint32_t u;
    memcpy(&u, &x, 4);
    return u;
}

float gx_f32_from_bits(uint32_t u) {
    float x;
    memcpy(&x, &u, 4);
    return x;
}
