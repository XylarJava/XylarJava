#ifndef XYLAR_GOOSE_CRYPTO_H
#define XYLAR_GOOSE_CRYPTO_H
#include "compression.h"
uint64_t gx_aes_open(gx_const_bytes key);
void gx_aes_close(uint64_t handle);
int32_t gx_aes_blocks(uint64_t handle, gx_const_bytes input, gx_bytes output);
int32_t gx_secure_random(gx_bytes output);
/* X.509 SubjectPublicKeyInfo; caller owns the resulting RSA handle. */
uint64_t gx_rsa_open(gx_const_bytes public_key);
void gx_rsa_close(uint64_t handle);
int64_t gx_rsa_size(uint64_t handle);
/* RSA/ECB/PKCS1Padding; result length, or -1 on failure. */
int64_t gx_rsa_encrypt(uint64_t handle, gx_const_bytes input, gx_bytes output);
#endif
