// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Handshake/status packets and packet_enum behavior, protocol 777.
// Full notice: ../licenses/upstream-MIT.txt.
import net.buf;
namespace packets263;
let PROTOCOL_VERSION: i32 = 777;
let STATUS: i32 = 1;
let LOGIN: i32 = 2;
let TRANSFER: i32 = 3;
struct Intention {
    known: bool = true, id: i32 = 0,
    protocol: i32 = 777, host: u8[] = [], port: u16 = 25565, intent: i32 = 2,
    payload: u8[] = [],
}
struct StatusPacket {
    known: bool = true, serverbound: bool = true, id: i32 = 0, time: i64 = 0,
    json: u8[] = [], payload: u8[] = [],
}
fn decode_intention(id: i32, payload: const u8[:]) -> Intention, wire::Error {
    if id != 0 { return Intention { known: false, id: id, payload: payload }, wire::Error.Ok; }
    var r = wire::reader(payload);
    let protocol, a = wire::read_varint(r);
    if !wire::is_ok(a) { return Intention {}, a; }
    let host, b = wire::read_string(r, 255);
    if !wire::is_ok(b) { return Intention {}, b; }
    let port, c = wire::read_u16(r);
    if !wire::is_ok(c) { return Intention {}, c; }
    let intent, d = wire::read_varint(r);
    if !wire::is_ok(d) { return Intention {}, d; }
    if wire::remaining(r) != 0 {
        return Intention {}, wire::Error.PacketTooLarge { id: id, serverbound: true, extra: wire::remaining(r) };
    }
    return Intention { protocol: protocol, host: host, port: port, intent: intent }, wire::Error.Ok;
}
fn encode_intention(packet: Intention&) -> u8[>..] {
    var output: u8[>..] = [];
    wire::write_varint(output, if packet.known { 0 } else { packet.id });
    if packet.known {
        wire::write_varint(output, packet.protocol);
        wire::write_string(output, packet.host);
        wire::write_u16(output, packet.port);
        wire::write_varint(output, packet.intent);
    } else { output.append(packet.payload); }
    return output;
}
fn decode_status(id: i32, payload: const u8[:], serverbound: bool) -> StatusPacket, wire::Error {
    if id != 0 && id != 1 {
        return StatusPacket { known: false, id: id, serverbound: serverbound, payload: payload }, wire::Error.Ok;
    }
    var r = wire::reader(payload);
    if id == 1 {
        let time, error = wire::read_i64(r);
        if !wire::is_ok(error) { return StatusPacket {}, error; }
        if wire::remaining(r) != 0 {
            return StatusPacket {}, wire::Error.PacketTooLarge { id: id, serverbound: serverbound, extra: wire::remaining(r) };
        }
        return StatusPacket { serverbound: serverbound, id: id, time: time }, wire::Error.Ok;
    }
    if serverbound {
        if wire::remaining(r) != 0 {
            return StatusPacket {}, wire::Error.PacketTooLarge { id: id, serverbound: true, extra: wire::remaining(r) };
        }
        return StatusPacket {}, wire::Error.Ok;
    }
    let json, error = wire::read_string(r, 32767);
    if !wire::is_ok(error) { return StatusPacket {}, error; }
    if wire::remaining(r) != 0 {
        return StatusPacket {}, wire::Error.PacketTooLarge { id: id, serverbound: false, extra: wire::remaining(r) };
    }
    return StatusPacket { serverbound: false, json: json }, wire::Error.Ok;
}

fn encode_status(packet: StatusPacket&) -> u8[>..] {
    var output: u8[>..] = [];
    wire::write_varint(output, packet.id);
    if !packet.known { output.append(packet.payload); }
    else if packet.id == 1 { wire::write_i64(output, packet.time); }
    else if !packet.serverbound { wire::write_string(output, packet.json); }
    return output;
}
