// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Adapted storage.rs tests; full notice: licenses/upstream-MIT.txt.
import core.storage;
import std;
fn main() {
    {
        var s, error = packed::new_storage(5, 13);
        assert(packed::is_ok(error) && s.words.len == 2);
        let old_a, a = packed::set(s, 11, 31);
        let old_b, b = packed::set(s, 12, 7);
        assert(packed::is_ok(a) && packed::is_ok(b) && old_a == 0 && old_b == 0);
        assert(s.words[0] == (31 as u64) << 55 && s.words[1] == 7);
    }
    for width in 1..33 {
        let bits = width as u8;
        var s, error = packed::new_storage(bits, 4096);
        assert(packed::is_ok(error));
        var expected: u32[>..] = [];
        for i in 4096 { expected.push(0); }
        for pass in 3 {
            for i in expected.len {
                let value = (((i * 7 + pass) as u64) & s.mask) as u32;
                let old, result = packed::set(s, i, value);
                assert(packed::is_ok(result) && old == expected[i]);
                expected[i] = value;
                let word, shift = packed::locate(bits, i);
                assert(word == i / s.per_word && shift == (i % s.per_word) * bits);
            }
            let values = packed::unpack(s);
            assert(values.len == expected.len);
            for i in values.len {
                assert(values[i] == expected[i] && packed::read(s, i) == expected[i]);
            }
            var into: u32[4099] = [4294967295; 4099];
            packed::unpack_into(s, into);
            for i in expected.len { assert(into[i] == expected[i]); }
            for i in 4096..4099 { assert(into[i] == 4294967295); }
        }
        var wide, wide_error = packed::new_storage(bits, packed::MAGIC_LEN + 1);
        assert(packed::is_ok(wide_error) && wide.wide);
        assert(packed::write(wide, packed::MAGIC_LEN, 1) == 0);
        assert(packed::read(wide, packed::MAGIC_LEN) == 1);
    }
    {
        let words: u64[1] = [18446744073709551615];
        var s, error = packed::from_words(5, 1, words);
        assert(packed::is_ok(error));
        let old, result = packed::set(s, 0, 0);
        assert(packed::is_ok(result) && old == 31 && s.words[0] == 18446744073709551584);
        let rejected, bounds = packed::set(s, 1, 0);
        let too_large, value_error = packed::set(s, 0, 32);
        assert(!packed::is_ok(bounds) && !packed::is_ok(value_error));
        assert(s.words[0] == 18446744073709551584);
        assert(!packed::is_ok(packed::fill_storage(s, 32)));
        assert(s.words[0] == 18446744073709551584);
    }
    for bits in [0, 33, 255] {
        let invalid, error = packed::new_storage(bits as u8, 1);
        assert(!packed::is_ok(error));
    }
    {
        let words: u64[1] = [0];
        let invalid, error = packed::from_words(5, 13, words);
        assert(!packed::is_ok(error));
        var empty, empty_error = packed::new_storage(1, 0);
        assert(packed::is_ok(empty_error) && packed::unpack(empty).len == 0);
    }
    {
        let words: u64[1] = [4294967295];
        var s, error = packed::from_words(32, 1, words);
        assert(packed::is_ok(error) && packed::read(s, 0) == 4294967295);
        let old, result = packed::set(s, 0, 4294967295);
        assert(!packed::is_ok(result) && packed::read(s, 0) == 4294967295);
        assert(packed::write(s, 0, 4294967294) == 4294967295);
        assert(packed::read(s, 0) == 4294967294);
    }
    print("Packed storage: golden words, 1..32-bit widths, overwrites, unpack, padding, bounds and atomic errors passed.");
}
