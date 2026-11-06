// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Port of protocol frame.rs. Full notice: ../licenses/upstream-MIT.txt.
// flate2 is adapted to system zlib through native/compression.c; policy is Goose.
import std;
import net.buf;
namespace framing;
extern fn gx_zlib_bound(length: i64) -> i64;
extern fn gx_deflate(input: const u8[:], output: u8[:], written: i64&) -> i32;
extern fn gx_inflater_open() -> u64;
extern fn gx_inflater_close(handle: u64);
extern fn gx_inflate(handle: u64, input: const u8[:], output: u8[:], written: i64&) -> i32;
let MAX_FRAME: i64 = (1 << 21) - 1;
let MAX_UNCOMPRESSED: i64 = 8388608;
enum Error {
    Ok,
    Wire { error: wire::Error },
    ZeroLength,
    TooWide,
    PacketTooLarge { size: i64 },
    NegativeSize,
    BelowThreshold { size: i64, threshold: i64 },
    AboveMaximum { size: i64 },
    InflateLength { actual: i64, declared: i64 },
    ZlibFailure { code: i32, inflating: bool },
}
fn is_ok(error: Error) -> bool { match error { Ok => true, _ => false } }
fn error_message(error: Error) -> u8[>..] {
    var out: u8[>..] = [];
    match error {
        Ok => {},
        Wire e => { out.append(wire::error_message(e.error)); },
        ZeroLength => { format(out, "Frame length cannot be zero"); },
        TooWide => { format(out, "length wider than 21 bits"); },
        PacketTooLarge e => { format(out, "packet too large: ", e.size, " bytes"); },
        NegativeSize => { format(out, "negative uncompressed length"); },
        BelowThreshold e => { format(out, "Badly compressed packet - size of ", e.size, " is below server threshold of ", e.threshold); },
        AboveMaximum e => { format(out, "Badly compressed packet - size of ", e.size, " is larger than protocol maximum"); },
        InflateLength e => { format(out, "Badly compressed packet - actual length ", e.actual, " does not match declared size ", e.declared); },
        ZlibFailure e => { format(out, if e.inflating { "Badly compressed packet - zlib status " } else { "zlib compression status " }, e.code); },
    }
    return out;
}
struct Header { complete: bool = false, start: i64 = 0, len: i64 = 0 }
fn header(bytes: const u8[:]) -> Header, Error {
    var length: i64 = 0;
    for i in 3 {
        if i >= bytes.len { return Header {}, Error.Ok; }
        let b = bytes[i];
        length |= ((b & 127) as i64) << (7 * i);
        if (b & 128) == 0 {
            if length == 0 { return Header {}, Error.ZeroLength; }
            return Header { complete: true, start: i + 1, len: length }, Error.Ok;
        }
    }
    return Header {}, Error.TooWide;
}
// -1 is Rust None; nonnegative thresholds are Some(threshold).
fn contents(body: const u8[:], threshold: i64 = -1) -> u8[>..], Error {
    assert(threshold >= -1);
    var inner: u8[>..] = [];
    if threshold < 0 { inner.append(body); }
    else if body.len < threshold { inner.push(0); inner.append(body); }
    else {
        let capacity = gx_zlib_bound(body.len);
        if capacity < 0 { return [], Error.ZlibFailure { code: -2, inflating: false }; }
        var compressed: u8[>..] = [];
        compressed.resize(capacity, 0);
        var written: i64 = 0;
        let status = gx_deflate(body, compressed, written);
        if status != 0 { return [], Error.ZlibFailure { code: status, inflating: false }; }
        wire::write_varint(inner, body.len as! i32);
        inner.append(compressed[..written]);
    }
    if inner.len > MAX_FRAME { return [], Error.PacketTooLarge { size: inner.len }; }
    return inner, Error.Ok;
}
fn prefixed(contents: const u8[:]) -> u8[>..] {
    var output: u8[>..] = [];
    wire::write_varint(output, contents.len as! i32);
    output.append(contents);
    return output;
}
fn encode(body: const u8[:], threshold: i64 = -1) -> u8[>..], Error {
    let inner, error = contents(body, threshold);
    if !is_ok(error) { return [], error; }
    return prefixed(inner), Error.Ok;
}
// A connection owns this handle exclusively. Close explicitly before its owner
// goes away; do not copy the handle or use it after close. No borrowed data lives in C.
struct Inflater { handle: u64 = 0 }
fn new_inflater() -> Inflater { Inflater { handle: gx_inflater_open() } }
fn close(inflater: Inflater&) {
    gx_inflater_close(inflater.handle);
    inflater.handle = 0;
}
struct Body { plain: bool = true, start: i64 = 0, data: u8[] = [] }
fn body(frame: const u8[:], threshold: i64, inflater: Inflater&) -> Body, Error {
    assert(threshold >= -1);
    if threshold < 0 { return Body {}, Error.Ok; }
    var reader = wire::reader(frame);
    let declared, read_error = wire::read_varint(reader);
    if !wire::is_ok(read_error) { return Body {}, Error.Wire { error: read_error }; }
    if declared == 0 { return Body { start: reader.pos }, Error.Ok; }
    if declared < 0 { return Body {}, Error.NegativeSize; }
    if declared < threshold { return Body {}, Error.BelowThreshold { size: declared, threshold: threshold }; }
    if declared > MAX_UNCOMPRESSED { return Body {}, Error.AboveMaximum { size: declared }; }
    var output: u8[>..] = [];
    output.resize((declared as i64) + 1, 0);
    var written: i64 = 0;
    let status = gx_inflate(inflater.handle, frame[reader.pos..], output, written);
    if status != 0 && status != 1 && status != -5 {
        return Body {}, Error.ZlibFailure { code: status, inflating: true };
    }
    if written != declared || status != 1 {
        return Body {}, Error.InflateLength { actual: written, declared: declared };
    }
    return Body { plain: false, data: output[..written] }, Error.Ok;
}
