// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Port of current-version buffering/framing from protocol connection.rs.
// POSIX TCP adapts std::net; SRV, pipe/split and older adapters remain separate.
// Full notice: ../licenses/upstream-MIT.txt.
import std;
import net.frame;
import net.crypto;
namespace transport;
extern fn gx_tcp_connect(host: const u8[:], port: u16, timeout_ms: i64, fd: i64&, error: i32&) -> i32;
extern fn gx_network_message(code: i32, lookup: i32, output: u8[:]) -> i64;
extern fn gx_network_kind(code: i32) -> i32;
extern fn gx_tcp_read(fd: i64, output: u8[:], timeout_ms: i64) -> i64;
extern fn gx_tcp_write(fd: i64, input: const u8[:]) -> i32;
extern fn gx_tcp_close(fd: i64);
struct Error { message: u8[] = [] }
fn is_ok(error: Error&) -> bool { error.message.len == 0 }
fn failure(message: const u8[:]) -> Error { Error { message: message } }
fn io_error(code: i32) -> Error {
    var message: u8[256] = [0; 256];
    let count = gx_network_message(code, 0, message);
    var text: u8[>..] = [];
    format(text, message[..count], " (os error ", code, ")");
    return failure(text);
}
struct Connection {
    fd: i64 = -1, threshold: i64 = -1, read_timeout_ms: i64 = -1,
    start: i64 = 0, end: i64 = 0,
    inflater: framing::Inflater = framing::Inflater {},
    encrypt: cryptography::Cfb8 = cryptography::Cfb8 {}, decrypt: cryptography::Cfb8 = cryptography::Cfb8 {},
    received: u64 = 0, sent: u64 = 0, bytes_in: u64 = 0, bytes_out: u64 = 0,
    buffer: u8[>..] = [],
}
// The connection owns the descriptor and inflater. Never copy an open owner.
fn new_detached() -> Connection { Connection { inflater: framing::new_inflater() } }
fn connect(host: const u8[:], port: u16, timeout_ms: i64) -> Connection, Error {
    assert(timeout_ms >= 0 && timeout_ms <= 2147483647);
    var fd: i64 = -1;
    var code: i32 = 0;
    let status = gx_tcp_connect(host, port, timeout_ms, fd, code);
    if status != 0 {
        var text: u8[>..] = [];
        if status == 1 {
            var message: u8[256] = [0; 256];
            let length = gx_network_message(code, 1, message);
            format(text, "Unknown host ", host, ": ", message[..length]);
        } else { let reason = io_error(code); format(text, "Connection refused: ", reason.message); }
        return Connection {}, failure(text);
    }
    return Connection { fd: fd, inflater: framing::new_inflater() }, Error {};
}
fn close(c: Connection&) {
    gx_tcp_close(c.fd); c.fd = -1;
    framing::close(c.inflater);
    cryptography::close(c.encrypt); cryptography::close(c.decrypt);
}
fn enable_encryption(c: Connection&, secret: const u8[:]) -> Error {
    assert(secret.len == 16);
    cryptography::close(c.encrypt); cryptography::close(c.decrypt);
    c.encrypt = cryptography::new_cipher(secret, secret);
    c.decrypt = cryptography::new_cipher(secret, secret);
    if c.encrypt.handle == 0 || c.decrypt.handle == 0 { return failure("Could not initialize AES-128"); }
    if !cryptography::decrypt(c.decrypt, c.buffer[c.start..c.end]) { return failure("AES decryption failed"); }
    return Error {};
}
fn set_compression(c: Connection&, threshold: i32) { c.threshold = if threshold < 0 { -1 } else { threshold }; }
fn set_read_timeout(c: Connection&, timeout_ms: i64) { assert(timeout_ms >= -1); c.read_timeout_ms = timeout_ms; }
struct Packet { id: i32 = 0, payload: u8[] = [] }
let PARTIAL: u8 = 0;
let EMPTY: u8 = 1;
let PACKET: u8 = 2;
struct Next { kind: u8 = PARTIAL, wanted: i64 = 0, packet: Packet = Packet {} }
fn packet_body(body: const u8[:]) -> Next, Error {
    if body.len == 0 { return Next { kind: EMPTY }, Error {}; }
    var reader = wire::reader(body);
    let id, error = wire::read_varint(reader);
    if !wire::is_ok(error) { return Next {}, failure(wire::error_message(error)); }
    return Next { kind: PACKET, packet: Packet { id: id, payload: body[reader.pos..] } }, Error {};
}
fn next(c: Connection&) -> Next, Error {
    let prefix, prefix_error = framing::header(c.buffer[c.start..c.end]);
    if !framing::is_ok(prefix_error) { return Next {}, failure(framing::error_message(prefix_error)); }
    if !prefix.complete { return Next {}, Error {}; }
    let wanted = prefix.start + prefix.len;
    if c.end - c.start < wanted { return Next { wanted: wanted }, Error {}; }
    let at = c.start + prefix.start;
    c.start += wanted;
    let body, body_error = framing::body(c.buffer[at..c.start], c.threshold, c.inflater);
    if !framing::is_ok(body_error) { return Next {}, failure(framing::error_message(body_error)); }
    if body.plain {
        let result, error = packet_body(c.buffer[at + body.start..c.start]);
        if is_ok(error) && result.kind == PACKET { c.received += 1; }
        return copy(result), copy(error);
    }
    let result, error = packet_body(body.data);
    if is_ok(error) && result.kind == PACKET { c.received += 1; }
    return copy(result), copy(error);
}
fn buffered(c: Connection&) -> Packet, bool, Error {
    loop {
        let result, error = next(c);
        if !is_ok(error) { return Packet {}, false, copy(error); }
        if result.kind == PACKET { return copy(result.packet), true, Error {}; }
        if result.kind == PARTIAL { return Packet {}, false, Error {}; }
    }
}
fn prepare(c: Connection&, frame: i64) {
    let pending = c.end - c.start;
    let room = max(65536, frame - pending);
    if c.buffer.len - c.end < room {
        for i in pending { c.buffer[i] = c.buffer[c.start + i]; }
        c.start = 0; c.end = pending;
        if c.buffer.len < pending + room { c.buffer.resize(pending + room, 0); }
    }
}
// Detached-reader adaptation: feed bytes obtained externally, without decoding
// ahead. Negotiated compression must apply to subsequent buffered frames.
fn feed(c: Connection&, bytes: const u8[:]) -> Error {
    prepare(c, bytes.len + c.end - c.start);
    for i in bytes.len { c.buffer[c.end + i] = bytes[i]; }
    c.bytes_in += bytes.len as u64;
    if c.decrypt.handle != 0 && !cryptography::decrypt(c.decrypt, c.buffer[c.end..c.end + bytes.len]) { return failure("AES decryption failed"); }
    c.end += bytes.len;
    return Error {};
}
fn recv(c: Connection&) -> Packet, Error {
    loop {
        let result, error = next(c);
        if !is_ok(error) { return Packet {}, copy(error); }
        if result.kind == PACKET { return copy(result.packet), Error {}; }
        if result.kind == EMPTY { continue; }
        prepare(c, result.wanted);
        let count = gx_tcp_read(c.fd, c.buffer[c.end..], c.read_timeout_ms);
        if count == 0 { return Packet {}, failure("Connection closed by the server"); }
        if count < 0 {
            let kind = gx_network_kind((-count) as i32);
            if kind == 1 { continue; }
            if kind == 2 { return Packet {}, failure("Timed out"); }
            return Packet {}, io_error((-count) as i32);
        }
        c.bytes_in += count as u64;
        if c.decrypt.handle != 0 && !cryptography::decrypt(c.decrypt, c.buffer[c.end..c.end + count]) { return Packet {}, failure("AES decryption failed"); }
        c.end += count;
    }
}
fn frame_into(c: Connection&, contents: const u8[:], output: u8[>..]&) -> Error {
    let at = output.len;
    wire::write_varint(output, contents.len as! i32);
    output.append(contents);
    if c.encrypt.handle != 0 && !cryptography::encrypt(c.encrypt, output[at..]) { return failure("AES encryption failed"); }
    c.sent += 1;
    c.bytes_out += (output.len - at) as u64;
    return Error {};
}
fn send_contents(c: Connection&, contents: const u8[:]) -> Error {
    var framed: u8[>..] = [];
    let error = frame_into(c, contents, framed);
    if !is_ok(error) { return copy(error); }
    let status = gx_tcp_write(c.fd, framed);
    if status != 0 { return io_error(status); }
    return Error {};
}
fn send(c: Connection&, body: const u8[:]) -> Error {
    let contents, error = framing::contents(body, c.threshold);
    if !framing::is_ok(error) { return failure(framing::error_message(error)); }
    return send_contents(c, contents);
}
