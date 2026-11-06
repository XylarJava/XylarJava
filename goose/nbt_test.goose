// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Golden binary NBT regressions from nbt.rs's unit tests.
import std;
import data.nbt;
fn add(d: nbt::Document&, kind: u8, bits: u64 = 0) -> i64 {
    let at = d.words.len; d.words.push(kind); d.words.push(bits); for i in 6 { d.words.push(0); } return at;
}
fn main() {
    // Synthetic compound wrappers count toward both reader and writer quotas.
    var mixed = nbt::Document {};
    let root = add(mixed, 9); mixed.root = root;
    let a = add(mixed, 3, 1); let b = add(mixed, 1, 2);
    mixed.words[root + 5] = 2; mixed.words[root + 6] = a as u64; mixed.words[a + 7] = b as u64;
    let encoded, e = nbt::write(mixed, nbt::Limits { max_nodes: 5 }); assert(nbt::is_ok(e));
    let expected: u8[19] = [9,10,0,0,0,2,3,0,0,0,0,0,1,0,1,0,0,2,0];
    // Exact valid 26.3 wire form, without unused trailing bytes.
    assert(encoded == expected);
    let too_small, quota_error = nbt::write(mixed, nbt::Limits { max_nodes: 4 });
    assert(quota_error.reason == "node limit exceeded");
    let read, used, read_error = nbt::read(encoded, nbt::Limits { max_nodes: 5 }); assert(nbt::is_ok(read_error) && used == encoded.len);
    let refused, rejected_at, rejected = nbt::read(encoded, nbt::Limits { max_nodes: 4 }); assert(!nbt::is_ok(rejected));

    // Disk output empties overlong modified UTF-8; network output rejects it.
    var long_string = nbt::Document {}; let string_node = add(long_string, 8); long_string.root = string_node;
    long_string.words[string_node + 4] = long_string.words.len as u64; long_string.words[string_node + 5] = 32768;
    for i in 32768 { long_string.words.push(0); }
    let disk, disk_error = nbt::write(long_string, nbt::Limits {}, true); assert(nbt::is_ok(disk_error));
    let empty_string: u8[5] = [8,0,0,0,0]; assert(disk == empty_string);
    let network, string_error = nbt::write(long_string); assert(string_error.reason == "modified UTF-8 exceeds 65535 bytes");
    // Disk NaNs canonicalize; network NaNs retain their payload and sign.
    var nan = nbt::Document {}; nan.root = add(nan, 5, 0xff800001);
    let wire_nan, wire_error = nbt::write(nan); assert(nbt::is_ok(wire_error));
    let raw: u8[5] = [5,255,128,0,1]; assert(wire_nan == raw);
    let named_nan, named_error = nbt::write(nan, nbt::Limits {}, true); assert(nbt::is_ok(named_error));
    let canonical: u8[7] = [5,0,0,127,192,0,0]; assert(named_nan == canonical);
    print("NBT: mixed-list wrapper quotas, overlong UTF-16 strings and disk/network NaN policy passed.");
}
