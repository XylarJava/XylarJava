// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Port of protocol packets/login.rs and its packet/codec behavior for 26.3.
// Full notice: ../licenses/upstream-MIT.txt. This is not an authenticated session.
import net.buf;
namespace login263;
struct Property { name: u8[] = [], value: u8[] = [], has_signature: bool = false, signature: u8[] = [] }
struct GameProfile { uuid: wire::Uuid = wire::Uuid {}, name: u8[] = [], properties: Property[] = [] }
enum Clientbound {
    Disconnect { reason_json: u8[] },
    Hello { server_id: u8[], public_key: u8[], challenge: u8[], should_authenticate: bool },
    LoginFinished { profile: GameProfile, session: wire::Uuid },
    Compression { threshold: i32 },
    CustomQuery { transaction: i32, channel: u8[], data: u8[] },
    CookieRequest { key: u8[] },
    Unknown { id: i32, payload: u8[] },
}
enum Serverbound {
    Hello { name: u8[], uuid: wire::Uuid },
    Key { key: u8[], challenge: u8[] },
    CustomQueryAnswer { transaction: i32, has_payload: bool, payload: u8[] },
    LoginAcknowledged,
    CookieResponse { key: u8[], has_payload: bool, payload: u8[] },
    Unknown { id: i32, payload: u8[] },
}
fn read_property(r: wire::Reader&) -> Property, wire::Error {
    let name, a = wire::read_string(r, 64);
    if !wire::is_ok(a) { return Property {}, a; }
    let value, b = wire::read_string(r, 32767);
    if !wire::is_ok(b) { return Property {}, b; }
    let present, c = wire::read_bool(r);
    if !wire::is_ok(c) { return Property {}, c; }
    if present {
        let signature, d = wire::read_string(r, 1024);
        if !wire::is_ok(d) { return Property {}, d; }
        return Property { name: name, value: value, has_signature: true, signature: signature }, wire::Error.Ok;
    }
    return Property { name: name, value: value }, wire::Error.Ok;
}
fn write_property(p: Property&, output: u8[>..]&) {
    wire::write_string(output, p.name);
    wire::write_string(output, p.value);
    wire::write_bool(output, p.has_signature);
    if p.has_signature { wire::write_string(output, p.signature); }
}
fn read_profile(r: wire::Reader&) -> GameProfile, wire::Error {
    let uuid, a = wire::read_uuid(r);
    if !wire::is_ok(a) { return GameProfile {}, a; }
    let name, b = wire::read_string(r, 16);
    if !wire::is_ok(b) { return GameProfile {}, b; }
    let count, c = wire::read_count(r, wire::remaining(r));
    if !wire::is_ok(c) { return GameProfile {}, c; }
    var properties: Property[>..] = [];
    for i in count {
        let p, error = read_property(r);
        if !wire::is_ok(error) { return GameProfile {}, error; }
        properties.push(copy(p));
    }
    return GameProfile { uuid: uuid, name: name, properties: properties[..] }, wire::Error.Ok;
}
fn write_profile(p: GameProfile&, output: u8[>..]&) {
    wire::write_uuid(output, p.uuid);
    wire::write_string(output, p.name);
    wire::write_varint(output, p.properties.len as! i32);
    for property in p.properties { write_property(property, output); }
}
fn client_id(p: Clientbound..&) -> i32 {
    match p { Disconnect v => 0, Hello v => 1, LoginFinished v => 2, Compression v => 3,
              CustomQuery v => 4, CookieRequest v => 5, Unknown v => v.id }
}
fn server_id(p: Serverbound..&) -> i32 {
    match p { Hello v => 0, Key v => 1, CustomQueryAnswer v => 2, LoginAcknowledged => 3,
              CookieResponse v => 4, Unknown v => v.id }
}
fn client_name(p: Clientbound..&) -> const u8[:] {
    match p { Disconnect v => "Disconnect", Hello v => "Hello", LoginFinished v => "LoginFinished",
              Compression v => "Compression", CustomQuery v => "CustomQuery", CookieRequest v => "CookieRequest", Unknown v => "Unknown" }
}
fn server_name(p: Serverbound..&) -> const u8[:] {
    match p { Hello v => "Hello", Key v => "Key", CustomQueryAnswer v => "CustomQueryAnswer",
              LoginAcknowledged => "LoginAcknowledged", CookieResponse v => "CookieResponse", Unknown v => "Unknown" }
}
fn client_placeholder() -> Clientbound.. { Clientbound.Unknown { id: 0, payload: [] } }
fn server_placeholder() -> Serverbound.. { Serverbound.Unknown { id: 0, payload: [] } }
fn read_clientbound(r: wire::Reader&, id: i32) -> Clientbound.., wire::Error {
    if id == 0 {
        let reason, e = wire::read_string(r, 262144);
        if !wire::is_ok(e) { return client_placeholder(), e; }
        return Clientbound.Disconnect { reason_json: reason }, wire::Error.Ok;
    }
    if id == 1 {
        let server_id, a = wire::read_string(r, 20);
        if !wire::is_ok(a) { return client_placeholder(), a; }
        let public_key, b = wire::read_byte_array(r, wire::remaining(r));
        if !wire::is_ok(b) { return client_placeholder(), b; }
        let challenge, c = wire::read_byte_array(r, wire::remaining(r));
        if !wire::is_ok(c) { return client_placeholder(), c; }
        let auth, d = wire::read_bool(r);
        if !wire::is_ok(d) { return client_placeholder(), d; }
        return Clientbound.Hello { server_id: server_id, public_key: public_key, challenge: challenge, should_authenticate: auth }, wire::Error.Ok;
    }
    if id == 2 {
        let profile, a = read_profile(r);
        if !wire::is_ok(a) { return client_placeholder(), a; }
        let session, b = wire::read_uuid(r);
        if !wire::is_ok(b) { return client_placeholder(), b; }
        return Clientbound.LoginFinished { profile: copy(profile), session: session }, wire::Error.Ok;
    }
    if id == 3 {
        let threshold, e = wire::read_varint(r);
        if !wire::is_ok(e) { return client_placeholder(), e; }
        return Clientbound.Compression { threshold: threshold }, wire::Error.Ok;
    }
    if id == 4 {
        let transaction, a = wire::read_varint(r);
        if !wire::is_ok(a) { return client_placeholder(), a; }
        let channel, b = wire::read_string(r, 32767);
        if !wire::is_ok(b) { return client_placeholder(), b; }
        return Clientbound.CustomQuery { transaction: transaction, channel: channel, data: wire::rest(r) }, wire::Error.Ok;
    }
    if id == 5 {
        let key, e = wire::read_string(r, 32767);
        if !wire::is_ok(e) { return client_placeholder(), e; }
        return Clientbound.CookieRequest { key: key }, wire::Error.Ok;
    }
    return Clientbound.Unknown { id: id, payload: wire::rest(r) }, wire::Error.Ok;
}
fn decode_clientbound(id: i32, payload: const u8[:]) -> Clientbound.., wire::Error {
    var reader = wire::reader(payload);
    let p, error = read_clientbound(reader, id);
    if !wire::is_ok(error) { return client_placeholder(), error; }
    if wire::remaining(reader) != 0 {
        return client_placeholder(), wire::Error.PacketTooLarge { id: id, serverbound: false, extra: wire::remaining(reader) };
    }
    return copy(p), wire::Error.Ok;
}
fn read_serverbound(r: wire::Reader&, id: i32) -> Serverbound.., wire::Error {
    if id == 0 {
        let name, a = wire::read_string(r, 16);
        if !wire::is_ok(a) { return server_placeholder(), a; }
        let uuid, b = wire::read_uuid(r);
        if !wire::is_ok(b) { return server_placeholder(), b; }
        return Serverbound.Hello { name: name, uuid: uuid }, wire::Error.Ok;
    }
    if id == 1 {
        let key, a = wire::read_byte_array(r, wire::remaining(r));
        if !wire::is_ok(a) { return server_placeholder(), a; }
        let challenge, b = wire::read_byte_array(r, wire::remaining(r));
        if !wire::is_ok(b) { return server_placeholder(), b; }
        return Serverbound.Key { key: key, challenge: challenge }, wire::Error.Ok;
    }
    if id == 2 {
        let transaction, a = wire::read_varint(r);
        if !wire::is_ok(a) { return server_placeholder(), a; }
        let present, b = wire::read_bool(r);
        if !wire::is_ok(b) { return server_placeholder(), b; }
        if present { return Serverbound.CustomQueryAnswer { transaction: transaction, has_payload: true, payload: wire::rest(r) }, wire::Error.Ok; }
        return Serverbound.CustomQueryAnswer { transaction: transaction, has_payload: false, payload: [] }, wire::Error.Ok;
    }
    if id == 3 { return Serverbound.LoginAcknowledged, wire::Error.Ok; }
    if id == 4 {
        let key, a = wire::read_string(r, 32767);
        if !wire::is_ok(a) { return server_placeholder(), a; }
        let present, b = wire::read_bool(r);
        if !wire::is_ok(b) { return server_placeholder(), b; }
        if present {
            let data, c = wire::read_byte_array(r, wire::remaining(r));
            if !wire::is_ok(c) { return server_placeholder(), c; }
            return Serverbound.CookieResponse { key: key, has_payload: true, payload: data }, wire::Error.Ok;
        }
        return Serverbound.CookieResponse { key: key, has_payload: false, payload: [] }, wire::Error.Ok;
    }
    return Serverbound.Unknown { id: id, payload: wire::rest(r) }, wire::Error.Ok;
}
fn decode_serverbound(id: i32, payload: const u8[:]) -> Serverbound.., wire::Error {
    var reader = wire::reader(payload);
    let p, error = read_serverbound(reader, id);
    if !wire::is_ok(error) { return server_placeholder(), error; }
    if wire::remaining(reader) != 0 {
        return server_placeholder(), wire::Error.PacketTooLarge { id: id, serverbound: true, extra: wire::remaining(reader) };
    }
    return copy(p), wire::Error.Ok;
}
fn encode_clientbound(p: Clientbound..&) -> u8[>..] {
    var out: u8[>..] = [];
    wire::write_varint(out, client_id(p));
    match p {
        Disconnect v => { wire::write_string(out, v.reason_json); },
        Hello v => { wire::write_string(out, v.server_id); wire::write_byte_array(out, v.public_key); wire::write_byte_array(out, v.challenge); wire::write_bool(out, v.should_authenticate); },
        LoginFinished v => { write_profile(v.profile, out); wire::write_uuid(out, v.session); },
        Compression v => { wire::write_varint(out, v.threshold); },
        CustomQuery v => { wire::write_varint(out, v.transaction); wire::write_string(out, v.channel); out.append(v.data); },
        CookieRequest v => { wire::write_string(out, v.key); },
        Unknown v => { out.append(v.payload); },
    }
    return out;
}
fn encode_serverbound(p: Serverbound..&) -> u8[>..] {
    var out: u8[>..] = [];
    wire::write_varint(out, server_id(p));
    match p {
        Hello v => { wire::write_string(out, v.name); wire::write_uuid(out, v.uuid); },
        Key v => { wire::write_byte_array(out, v.key); wire::write_byte_array(out, v.challenge); },
        CustomQueryAnswer v => { wire::write_varint(out, v.transaction); wire::write_bool(out, v.has_payload); if v.has_payload { out.append(v.payload); } },
        LoginAcknowledged => {},
        CookieResponse v => { wire::write_string(out, v.key); wire::write_bool(out, v.has_payload); if v.has_payload { wire::write_byte_array(out, v.payload); } },
        Unknown v => { out.append(v.payload); },
    }
    return out;
}
