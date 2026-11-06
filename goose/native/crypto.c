/* OpenSSL adapts the original external AES and OS randomness dependencies.
 * CFB8 registers and packet encryption policy live in Goose. */
#include "crypto.h"
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
uint64_t gx_aes_open(gx_const_bytes key) {
    if (key.len != 16) return 0;
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (!context) return 0;
    if (EVP_EncryptInit_ex(context, EVP_aes_128_ecb(), NULL, key.data, NULL) != 1 ||
        EVP_CIPHER_CTX_set_padding(context, 0) != 1) {
        EVP_CIPHER_CTX_free(context); return 0;
    }
    return (uint64_t)(uintptr_t)context;
}
void gx_aes_close(uint64_t handle) {
    if (handle) EVP_CIPHER_CTX_free((EVP_CIPHER_CTX *)(uintptr_t)handle);
}
int32_t gx_aes_blocks(uint64_t handle, gx_const_bytes input, gx_bytes output) {
    if (!handle || input.len < 0 || input.len > INT_MAX || input.len % 16 || output.len < input.len) return 0;
    int written = 0;
    return EVP_EncryptUpdate((EVP_CIPHER_CTX *)(uintptr_t)handle, output.data, &written, input.data,
                            (int)input.len) == 1 && written == input.len;
}
int32_t gx_secure_random(gx_bytes output) {
    return output.len >= 0 && output.len <= INT_MAX && RAND_bytes(output.data, (int)output.len) == 1;
}

/* Native dependency adaptation of rsa.rs's public-key operation. Packet
 * ordering and encryption negotiation remain in the Goose session module. */
uint64_t gx_rsa_open(gx_const_bytes public_key) {
    if (public_key.len <= 0 || public_key.len > LONG_MAX) return 0;
    const unsigned char *at = public_key.data;
    EVP_PKEY *key = d2i_PUBKEY(NULL, &at, (long)public_key.len);
    if (!key) return 0;
    if (EVP_PKEY_is_a(key, "RSA") != 1) {
        EVP_PKEY_free(key);
        return 0;
    }
    return (uint64_t)(uintptr_t)key;
}
void gx_rsa_close(uint64_t handle) {
    if (handle) EVP_PKEY_free((EVP_PKEY *)(uintptr_t)handle);
}
int64_t gx_rsa_size(uint64_t handle) {
    return handle ? EVP_PKEY_get_size((EVP_PKEY *)(uintptr_t)handle) : 0;
}
int64_t gx_rsa_encrypt(uint64_t handle, gx_const_bytes input, gx_bytes output) {
    if (!handle || input.len < 0 || output.len < 0) return -1;
    int capacity = EVP_PKEY_get_size((EVP_PKEY *)(uintptr_t)handle);
    if (capacity <= 11 || input.len > capacity - 11 || output.len < capacity) return -1;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new((EVP_PKEY *)(uintptr_t)handle, NULL);
    if (!ctx) return -1;
    size_t size = (size_t)output.len;
    int ok = EVP_PKEY_encrypt_init(ctx) > 0 &&
             EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0 &&
             EVP_PKEY_encrypt(ctx, output.data, &size, input.data, (size_t)input.len) > 0;
    EVP_PKEY_CTX_free(ctx);
    return ok && size <= (size_t)output.len ? (int64_t)size : -1;
}
