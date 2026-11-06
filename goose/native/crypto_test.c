/* Dependency-adapter tests: no game accounts, sockets or player files. */
#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "failed: %s:%d: %s\n", __FILE__, __LINE__, #c); abort(); } } while (0)
static gx_const_bytes input(const uint8_t *p, size_t n) { return (gx_const_bytes){p, (int64_t)n}; }
static gx_bytes output(uint8_t *p, size_t n) { return (gx_bytes){p, (int64_t)n}; }
static void rsa_round_trip(int bits) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
    EVP_PKEY *private = NULL;
    CHECK(ctx && EVP_PKEY_keygen_init(ctx) > 0);
    CHECK(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) > 0);
    CHECK(EVP_PKEY_keygen(ctx, &private) > 0);
    EVP_PKEY_CTX_free(ctx);
    unsigned char *der = NULL;
    int n = i2d_PUBKEY(private, &der);
    CHECK(n > 0);
    uint64_t key = gx_rsa_open(input(der, (size_t)n));
    CHECK(key && gx_rsa_size(key) == bits / 8);
    CHECK(!gx_rsa_open(input(der, 5)));
    uint8_t message[512], encrypted[512], plain[512];
    for (size_t i = 0; i < sizeof(message); ++i) message[i] = (uint8_t)i;
    size_t sizes[] = {0, 16, (size_t)bits / 8 - 11};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        CHECK(gx_rsa_encrypt(key, input(message, sizes[i]), output(encrypted, sizeof(encrypted))) == bits / 8);
        ctx = EVP_PKEY_CTX_new(private, NULL);
        CHECK(ctx && EVP_PKEY_decrypt_init(ctx) > 0);
        CHECK(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0);
        size_t used = sizeof(plain);
        CHECK(EVP_PKEY_decrypt(ctx, plain, &used, encrypted, (size_t)bits / 8) > 0);
        CHECK(used == sizes[i] && memcmp(plain, message, used) == 0);
        EVP_PKEY_CTX_free(ctx);
    }
    CHECK(gx_rsa_encrypt(key, input(message, (size_t)bits / 8 - 10), output(encrypted, sizeof(encrypted))) == -1);
    CHECK(gx_rsa_encrypt(key, input(message, 16), output(encrypted, (size_t)bits / 8 - 1)) == -1);
    gx_rsa_close(key);
    OPENSSL_free(der);
    EVP_PKEY_free(private);
}
int main(void) {
    const uint8_t key[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    const uint8_t block[16] = {0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    const uint8_t expected[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    uint8_t data[16];
    uint64_t aes = gx_aes_open(input(key, sizeof(key)));
    CHECK(aes && gx_aes_blocks(aes, input(block, sizeof(block)), output(data, sizeof(data))) == 1);
    CHECK(memcmp(data, expected, sizeof(data)) == 0);
    CHECK(!gx_aes_open(input(key, 15)));
    CHECK(!gx_aes_blocks(aes, input(block, 15), output(data, sizeof(data))));
    CHECK(!gx_aes_blocks(aes, input(block, 16), output(data, 15)));
    gx_aes_close(aes);
    gx_aes_close(0);
    CHECK(!gx_rsa_open(input((const uint8_t *)"bad", 3)));
    CHECK(!gx_rsa_open(input(key, 0)) && gx_rsa_size(0) == 0);
    CHECK(gx_rsa_encrypt(0, input(key, 16), output(data, 16)) == -1);
    gx_rsa_close(0);
    EVP_PKEY *ec = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "prime256v1");
    CHECK(ec);
    unsigned char *der = NULL;
    int n = i2d_PUBKEY(ec, &der);
    CHECK(n > 0 && !gx_rsa_open(input(der, (size_t)n)));
    OPENSSL_free(der);
    EVP_PKEY_free(ec);
    rsa_round_trip(1024);
    rsa_round_trip(2048);
    puts("Native crypto: FIPS-197 AES, RSA PKCS1 interoperability, invalid inputs and buffer bounds passed.");
    return 0;
}
