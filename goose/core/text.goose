// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Rust-to-Goose adaptation. Full notice: ../licenses/upstream-MIT.txt.
// Reference: text.rs in ../upstream.lock.json.
import std;
namespace java_text;

let ZEROES: i64[37] = [
    0x30, 0x660, 0x6f0, 0x7c0, 0x966, 0x9e6, 0xa66, 0xae6, 0xb66, 0xbe6, 0xc66, 0xce6, 0xd66,
    0xde6, 0xe50, 0xed0, 0xf20, 0x1040, 0x1090, 0x17e0, 0x1810, 0x1946, 0x19d0, 0x1a80, 0x1a90,
    0x1b50, 0x1bb0, 0x1c40, 0x1c50, 0xa620, 0xa8d0, 0xa900, 0xa9d0, 0xa9f0, 0xaa50, 0xabf0, 0xff10,
];

// Rust takes a validated str. Goose takes bytes, so reject malformed UTF-8 as
// well as non-digit scalars instead of using std's permissive each_utf8.
fn parse_java_integer(text: u8[:]) -> i32, bool {
    var i: i64 = 0;
    var negative = false;
    if text.len > 0 && (text[0] == '-' || text[0] == '+') {
        negative = text[0] == '-';
        i = 1;
    }
    if i == text.len { return 0, false; }
    var value: i64 = 0;
    let limit: i64 = 2147483647 + (if negative { 1 } else { 0 });
    while i < text.len {
        let b: i64 = text[i];
        var cp = b;
        if b < 0x80 { i += 1; }
        else if b >= 0xc2 && b <= 0xdf {
            if i + 1 >= text.len || text[i + 1] < 0x80 || text[i + 1] > 0xbf { return 0, false; }
            cp = ((b & 31) << 6) | (text[i + 1] & 63);
            i += 2;
        } else if b >= 0xe0 && b <= 0xef {
            if i + 2 >= text.len { return 0, false; }
            let second = text[i + 1];
            let third = text[i + 2];
            if second < 0x80 || second > 0xbf || third < 0x80 || third > 0xbf ||
               (b == 0xe0 && second < 0xa0) || (b == 0xed && second >= 0xa0) { return 0, false; }
            cp = ((b & 15) << 12) | (((second as i64) & 63) << 6) | (third & 63);
            i += 3;
        } else { return 0, false; } // Accepted digits are all BMP scalars.
        var digit: i64 = -1;
        for zero in ZEROES {
            if cp >= zero && cp < zero + 10 { digit = cp - zero; break; }
        }
        if digit < 0 { return 0, false; }
        value = value * 10 + digit;
        if value > limit { return 0, false; }
    }
    return (if negative { -value } else { value }) as i32, true;
}
