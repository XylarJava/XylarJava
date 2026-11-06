// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Port of crypto.rs CFB8 state and MD5/offline UUID helpers.
// AES blocks/secure randomness adapt external dependencies to OpenSSL.
// Full notice: ../licenses/upstream-MIT.txt.
import std;
import net.buf;
namespace cryptography;
extern fn gx_aes_open(key: const u8[:]) -> u64;
extern fn gx_aes_close(handle: u64);
extern fn gx_aes_blocks(handle: u64, input: const u8[:], output: u8[:]) -> i32;
extern fn gx_secure_random(output: u8[:]) -> i32;
extern fn gx_rsa_open(public_key: const u8[:]) -> u64;
extern fn gx_rsa_close(handle: u64);
extern fn gx_rsa_size(handle: u64) -> i64;
extern fn gx_rsa_encrypt(handle: u64, input: const u8[:], output: u8[:]) -> i64;
let LANES: i64 = 32;
struct Cfb8 { handle: u64 = 0, register: u8[16] = [0; 16] }
fn new_cipher(key: const u8[:], iv: const u8[:]) -> Cfb8 {
    assert(key.len == 16 && iv.len == 16);
    var c = Cfb8 { handle: gx_aes_open(key) };
    for i in 16 { c.register[i] = iv[i]; }
    return c;
}
fn close(c: Cfb8&) { gx_aes_close(c.handle); c.handle = 0; }
fn encrypt(c: Cfb8&, data: u8[:]) -> bool {
    for i in data.len {
        var encrypted = c.register;
        if gx_aes_blocks(c.handle, encrypted, encrypted) != 1 { return false; }
        data[i] ^= encrypted[0];
        for j in 15 { c.register[j] = c.register[j + 1]; }
        c.register[15] = data[i];
    }
    return true;
}
fn decrypt(c: Cfb8&, data: u8[:]) -> bool {
    var at: i64 = 0;
    while at < data.len {
        let count = min(LANES, data.len - at);
        var window: u8[48] = [0; 48];
        for i in 16 { window[i] = c.register[i]; }
        for i in count { window[16 + i] = data[at + i]; }
        var blocks: u8[512] = [0; 512];
        for i in LANES { for j in 16 { blocks[i * 16 + j] = window[i + j]; } }
        if gx_aes_blocks(c.handle, blocks, blocks) != 1 { return false; }
        for i in count { data[at + i] ^= blocks[i * 16]; }
        for i in 16 { c.register[i] = window[count + i]; }
        at += count;
    }
    return true;
}
fn secure_random(output: u8[:]) -> bool { gx_secure_random(output) == 1 }
// OpenSSL adapts rsa.rs's public-key dependency. This is not a translation of
// its big-integer arithmetic. A handle has one owner and explicit cleanup.
struct RsaPublicKey { handle: u64 = 0 }
fn public_key(der: const u8[:]) -> RsaPublicKey { RsaPublicKey { handle: gx_rsa_open(der) } }
fn close(key: RsaPublicKey&) { gx_rsa_close(key.handle); key.handle = 0; }
fn encrypt(key: RsaPublicKey&, input: const u8[:]) -> u8[>..], bool {
    var out: u8[>..] = [];
    let size = gx_rsa_size(key.handle);
    if size <= 11 || input.len > size - 11 { return out, false; }
    out.resize(size, 0);
    let written = gx_rsa_encrypt(key.handle, input, out);
    if written < 0 { out.clear(); return out, false; }
    out.resize(written, 0);
    return out, true;
}
fn rotate_left(v: u32, shift: u32) -> u32 { (v << shift) | (v >> (32 - shift)) }
let S: u32[64] = [7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21];
// The reference recomputes floor(abs(sin(i+1))*2^32) on each invocation.
// These exact constants avoid repeated transcendental work.
let K: u32[64] = [
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391];
fn md5(input: const u8[:]) -> u8[16] {
    var a0: u32 = 0x67452301; var b0: u32 = 0xefcdab89;
    var c0: u32 = 0x98badcfe; var d0: u32 = 0x10325476;
    var msg: u8[>..] = [];
    msg.append(input); msg.push(128);
    while msg.len % 64 != 56 { msg.push(0); }
    let length = (input.len as u64) * 8;
    for i in 8 { msg.push((length >> (8 * i)) as! u8); }
    var at: i64 = 0;
    while at < msg.len {
        var m: u32[16] = [0; 16];
        for i in 16 { for j in 4 { m[i] |= (msg[at + i * 4 + j] as u32) << (j * 8); } }
        var a = a0; var b = b0; var c = c0; var d = d0;
        for i in 64 {
            var f: u32 = 0; var g: i64 = 0;
            if i / 16 == 0 { f = (b & c) | (~b & d); g = i; }
            else if i / 16 == 1 { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if i / 16 == 2 { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            f += a + K[i] + m[g];
            a = d; d = c; c = b; b += rotate_left(f, S[i]);
        }
        a0 += a; b0 += b; c0 += c; d0 += d; at += 64;
    }
    let words: u32[4] = [a0,b0,c0,d0];
    var output: u8[16] = [0; 16];
    for i in 4 { for j in 4 { output[i * 4 + j] = (words[i] >> (8 * j)) as! u8; } }
    return output;
}
fn offline_uuid(name: const u8[:]) -> wire::Uuid {
    var input: u8[>..] = [];
    input.append("OfflinePlayer:"); input.append(name);
    var h = md5(input);
    h[6] = (h[6] & 15) | 48; h[8] = (h[8] & 63) | 128;
    return wire::Uuid { high: wire::decode_uint(h[..8]), low: wire::decode_uint(h[8..]) };
}
fn uuid_string(uuid: wire::Uuid) -> u8[>..] {
    var raw: u8[>..] = [];
    wire::write_uuid(raw, uuid);
    let digits: const u8[:] = "0123456789abcdef";
    var out: u8[>..] = [];
    for i in 16 {
        if i == 4 || i == 6 || i == 8 || i == 10 { out.push('-'); }
        out.push(digits[raw[i] >> 4]); out.push(digits[raw[i] & 15]);
    }
    return out;
}
