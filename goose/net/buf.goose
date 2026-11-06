// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Rust buf.rs primitive port. Full notice: ../licenses/upstream-MIT.txt.
// Floating-point bitcasts remain pending; NBT is adapted to canonical bytes.
import std;
import data.nbt;
namespace wire;

enum Error {
    Ok,
    NeedBytes { needed: i64, remaining: i64 },
    VarIntTooBig,
    VarLongTooBig,
    NegativeCount { count: i32 },
    CountTooLarge { count: i64, maximum: i64 },
    BadUtf8 { valid_up_to: i64, error_len: i64 },
    StringTooLong { maximum: i64 },
    FixedLongsPastEnd,
    Nbt { offset: i64, reason: const u8[:] },
    InvalidBits { bits: u8 },
    InvalidLayer { length: i64 },
    PacketTooLarge { id: i32, serverbound: bool, extra: i64 },
}
fn is_ok(error: Error) -> bool { match error { Ok => true, _ => false } }
fn error_message(error: Error) -> u8[>..] {
    var out: u8[>..] = [];
    match error {
        Ok => {},
        NeedBytes e => { format(out, "needed ", e.needed, " bytes, ", e.remaining, " left"); },
        VarIntTooBig => { format(out, "VarInt too big"); },
        VarLongTooBig => { format(out, "VarLong too big"); },
        NegativeCount e => { format(out, "negative count ", e.count); },
        CountTooLarge e => { format(out, e.count, " elements is more than allowed ", e.maximum); },
        BadUtf8 e => {
            if e.error_len < 0 { format(out, "bad UTF-8: incomplete utf-8 byte sequence from index ", e.valid_up_to); }
            else { format(out, "bad UTF-8: invalid utf-8 sequence of ", e.error_len, " bytes from index ", e.valid_up_to); }
        },
        StringTooLong e => { format(out, "string longer than ", e.maximum, " chars"); },
        Nbt e => { format(out, "NBT: NBT at byte ", e.offset, ": ", e.reason); },
        FixedLongsPastEnd => { format(out, "fixed long array past the end"); },
        InvalidLayer e => { format(out, "DataLayer should be 2048 bytes not: ", e.length); },
        InvalidBits e => { format(out, e.bits, " bits per entry, more than 32"); },
        PacketTooLarge e => {
            format(out, "packet ", e.id, " (", if e.serverbound { "Serverbound" } else { "Clientbound" },
                   ") was larger than expected: ", e.extra, " extra bytes");
        },
    }
    return out;
}
struct Reader { data: const u8[:], pos: i64 = 0 }
struct Uuid { high: u64 = 0, low: u64 = 0 }
fn reader(data: const u8[:]) -> Reader { Reader { data: data } }
fn remaining(r: Reader&) -> i64 { r.data.len - r.pos }
fn since(r: Reader&, start: i64) -> const u8[:] { r.data[min(start, r.pos)..r.pos] }
fn rest(r: Reader&) -> const u8[:] {
    let start = r.pos;
    r.pos = r.data.len;
    return r.data[start..];
}
fn read_bytes(r: Reader&, n: i64) -> const u8[:], Error {
    assert(n >= 0);
    if remaining(r) < n { return "", Error.NeedBytes { needed: n, remaining: remaining(r) }; }
    let start = r.pos;
    r.pos += n;
    return r.data[start..r.pos], Error.Ok;
}
fn decode_uint(bytes: const u8[:]) -> u64 {
    var value: u64 = 0;
    for b in bytes { value = (value << 8) | b; }
    return value;
}
fn read_uint(r: Reader&, width: i64) -> u64, Error {
    let bytes, error = read_bytes(r, width);
    if !is_ok(error) { return 0, error; }
    return decode_uint(bytes), Error.Ok;
}
fn read_u8(r: Reader&) -> u8, Error { let v, e = read_uint(r, 1); return v as u8, e; }
fn read_i8(r: Reader&) -> i8, Error { let v, e = read_uint(r, 1); return v as! i8, e; }
fn read_bool(r: Reader&) -> bool, Error { let v, e = read_u8(r); return v != 0, e; }
fn read_u16(r: Reader&) -> u16, Error { let v, e = read_uint(r, 2); return v as u16, e; }
fn read_i16(r: Reader&) -> i16, Error { let v, e = read_uint(r, 2); return v as! i16, e; }
fn read_u32(r: Reader&) -> u32, Error { let v, e = read_uint(r, 4); return v as u32, e; }
fn read_i32(r: Reader&) -> i32, Error { let v, e = read_uint(r, 4); return v as! i32, e; }
fn read_i64(r: Reader&) -> i64, Error { let v, e = read_uint(r, 8); return v as! i64, e; }
fn read_uuid(r: Reader&) -> Uuid, Error {
    let bytes, error = read_bytes(r, 16);
    if !is_ok(error) { return Uuid {}, error; }
    return Uuid { decode_uint(bytes[0..8]), decode_uint(bytes[8..16]) }, Error.Ok;
}
fn read_varint(r: Reader&) -> i32, Error {
    var value: u32 = 0;
    for i in 5 {
        let b, error = read_u8(r);
        if !is_ok(error) { return 0, error; }
        value |= ((b & 127) as u32) << (7 * i);
        if (b & 128) == 0 { return value as! i32, Error.Ok; }
    }
    return 0, Error.VarIntTooBig;
}
fn read_varlong(r: Reader&) -> i64, Error {
    var value: u64 = 0;
    for i in 10 {
        let b, error = read_u8(r);
        if !is_ok(error) { return 0, error; }
        value |= ((b & 127) as u64) << (7 * i);
        if (b & 128) == 0 { return value as! i64, Error.Ok; }
    }
    return 0, Error.VarLongTooBig;
}
fn read_count(r: Reader&, maximum: i64) -> i64, Error {
    assert(maximum >= 0);
    let value, error = read_varint(r);
    if !is_ok(error) { return 0, error; }
    if value < 0 { return 0, Error.NegativeCount { count: value }; }
    if value > maximum { return 0, Error.CountTooLarge { count: value, maximum: maximum }; }
    return value, Error.Ok;
}
// Strict UTF-8 with Rust Utf8Error's position/length and Java UTF-16 unit count.
fn utf16_units(text: const u8[:]) -> i64, Error {
    var index: i64 = 0;
    var count: i64 = 0;
    while index < text.len {
        let lead = text[index];
        if lead < 128 { index += 1; count += 1; continue; }
        let width = if lead >= 0xc2 && lead <= 0xdf { 2 }
                    else if lead >= 0xe0 && lead <= 0xef { 3 }
                    else if lead >= 0xf0 && lead <= 0xf4 { 4 } else { 0 };
        if width == 0 { return 0, Error.BadUtf8 { valid_up_to: index, error_len: 1 }; }
        for offset in 1..width {
            if index + offset >= text.len {
                return 0, Error.BadUtf8 { valid_up_to: index, error_len: -1 };
            }
            let byte = text[index + offset];
            var valid = byte >= 128 && byte <= 191;
            if offset == 1 {
                if lead == 0xe0 { valid = byte >= 0xa0 && byte <= 0xbf; }
                if lead == 0xed { valid = byte >= 0x80 && byte <= 0x9f; }
                if lead == 0xf0 { valid = byte >= 0x90 && byte <= 0xbf; }
                if lead == 0xf4 { valid = byte >= 0x80 && byte <= 0x8f; }
            }
            if !valid { return 0, Error.BadUtf8 { valid_up_to: index, error_len: offset }; }
        }
        index += width;
        count += if width == 4 { 2 } else { 1 };
    }
    return count, Error.Ok;
}
fn read_string(r: Reader&, maximum: i64) -> const u8[:], Error {
    let byte_limit = if maximum > 9223372036854775807 / 3 { 9223372036854775807 } else { maximum * 3 };
    let length, error = read_count(r, byte_limit);
    if !is_ok(error) { return "", error; }
    let bytes, bytes_error = read_bytes(r, length);
    if !is_ok(bytes_error) { return "", bytes_error; }
    let units, utf8_error = utf16_units(bytes);
    if !is_ok(utf8_error) { return "", utf8_error; }
    if units > maximum { return "", Error.StringTooLong { maximum: maximum }; }
    return bytes, Error.Ok;
}
fn read_byte_array(r: Reader&, maximum: i64) -> const u8[:], Error {
    let length, error = read_count(r, maximum);
    if !is_ok(error) { return "", error; }
    return read_bytes(r, length);
}
fn read_fixed_longs(r: Reader&, count: i64) -> i64[>..], Error {
    assert(count >= 0);
    if count > remaining(r) / 8 { return [], Error.FixedLongsPastEnd; }
    var values: i64[>..] = [];
    for i in count {
        let value, error = read_i64(r);
        if !is_ok(error) { return [], error; }
        values.push(value);
    }
    return values, Error.Ok;
}
fn read_long_array(r: Reader&) -> i64[>..], Error {
    let count, error = read_count(r, remaining(r) / 8);
    if !is_ok(error) { return [], error; }
    var values: i64[>..] = [];
    for i in count {
        let value, item_error = read_i64(r);
        if !is_ok(item_error) { return [], item_error; }
        values.push(value);
    }
    return values, Error.Ok;
}
fn write_uint(output: u8[>..]&, value: u64, width: i64) {
    for i in width { output.push((value >> ((width - i - 1) * 8)) as! u8); }
}
fn write_u8(output: u8[>..]&, value: u8) { output.push(value); }
fn write_i8(output: u8[>..]&, value: i8) { output.push(value as! u8); }
fn write_bool(output: u8[>..]&, value: bool) { output.push(if value { 1 } else { 0 }); }
fn write_u16(output: u8[>..]&, value: u16) { write_uint(output, value, 2); }
fn write_i16(output: u8[>..]&, value: i16) { write_uint(output, value as! u64, 2); }
fn write_u32(output: u8[>..]&, value: u32) { write_uint(output, value, 4); }
fn write_i32(output: u8[>..]&, value: i32) { write_uint(output, value as! u64, 4); }
fn write_i64(output: u8[>..]&, value: i64) { write_uint(output, value as! u64, 8); }
fn write_uuid(output: u8[>..]&, value: Uuid) { write_uint(output, value.high, 8); write_uint(output, value.low, 8); }
fn write_varint(output: u8[>..]&, input: i32) {
    var value = input as! u32;
    loop {
        if (value & ~127) == 0 { output.push(value as u8); return; }
        output.push(((value as! u8) & 127) | 128);
        value >>= 7;
    }
}
fn write_varlong(output: u8[>..]&, input: i64) {
    var value = input as! u64;
    loop {
        if (value & ~127) == 0 { output.push(value as u8); return; }
        output.push(((value as! u8) & 127) | 128);
        value >>= 7;
    }
}
fn varint_size(value: i32) -> i64 {
    let bits = value as! u32;
    for i in 1..5 { if (bits >> (7 * i)) == 0 { return i; } }
    return 5;
}
fn write_string(output: u8[>..]&, text: const u8[:]) { write_varint(output, text.len as! i32); output.append(text); }
fn write_byte_array(output: u8[>..]&, bytes: const u8[:]) { write_varint(output, bytes.len as! i32); output.append(bytes); }
fn write_long_array(output: u8[>..]&, values: i64[:]) {
    write_varint(output, values.len as! i32);
    for value in values { write_i64(output, value); }
}
fn bit(bytes: const u8[:], index: i64) -> bool {
    assert(index >= 0);
    return index / 8 < bytes.len && ((bytes[index / 8] >> (index % 8)) & 1) != 0;
}
fn set_bit(bytes: u8[>..]&, index: i64) {
    assert(index >= 0);
    while bytes.len <= index / 8 { bytes.push(0); }
    bytes[index / 8] |= 1 << (index % 8);
}
fn write_bitset(output: u8[>..]&, bytes: const u8[:]) {
    var length = bytes.len;
    while length > 0 && bytes[length - 1] == 0 { length -= 1; }
    write_byte_array(output, bytes[0..length]);
}

// Network NBT is exposed as canonical bytes, with validation/normalization by
// the translated NBT tree. The owning arena can be reconstructed on demand.
fn read_nbt(r: Reader&) -> u8[>..], Error {
    var document, used, error = nbt::read(r.data[r.pos..], nbt::Limits { max_bytes: 2097152 });
    if !nbt::is_ok(error) { return [], Error.Nbt { offset: error.offset, reason: error.reason }; }
    r.pos += used;
    let bytes, output_error = nbt::write(document);
    if !nbt::is_ok(output_error) { return [], Error.Nbt { offset: output_error.offset, reason: output_error.reason }; }
    return bytes, Error.Ok;
}
fn write_nbt(output: u8[>..]&, bytes: const u8[:]) { output.append(bytes); }
