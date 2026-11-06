// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Rust-to-Goose adaptation. Full notice: ../licenses/upstream-MIT.txt.
// Reference: biome/seed_hash.rs in ../upstream.lock.json.
namespace biome_seed;

let K: u32[64] = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
];

fn rotate_right(value: u32, amount: i64) -> u32 { (value >> amount) | (value << (32 - amount)) }
fn swap32(value: u32) -> u32 {
    ((value & 255) << 24) | ((value & 65280) << 8) |
    ((value >> 8) & 65280) | (value >> 24)
}
// SHA-256 specialized to one little-endian signed world seed. Return the
// first eight digest bytes interpreted as a little-endian signed long.
fn hash(seed: i64) -> i64 {
    let bits = seed as! u64;
    var words: u32[64] = [0; 64];
    words[0] = swap32(bits as! u32);
    words[1] = swap32((bits >> 32) as u32);
    words[2] = 0x80000000;
    words[15] = 64;
    for i in 16..64 {
        let a = words[i - 15];
        let b = words[i - 2];
        let small0 = rotate_right(a, 7) ^ rotate_right(a, 18) ^ (a >> 3);
        let small1 = rotate_right(b, 17) ^ rotate_right(b, 19) ^ (b >> 10);
        words[i] = words[i - 16] + small0 + words[i - 7] + small1;
    }
    let initial: u32[8] = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
    var a = initial[0];
    var b = initial[1];
    var c = initial[2];
    var d = initial[3];
    var e = initial[4];
    var f = initial[5];
    var g = initial[6];
    var h = initial[7];
    for i in 64 {
        let sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        let choice = (e & f) ^ (~e & g);
        let t1 = h + sum1 + choice + K[i] + words[i];
        let sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        let majority = (a & b) ^ (a & c) ^ (b & c);
        let t2 = sum0 + majority;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    let first = swap32(a + initial[0]);
    let second = swap32(b + initial[1]);
    return (first | ((second as u64) << 32)) as! i64;
}
