import std;
import os;
import net.login;
fn hex(bytes: const u8[:]) -> u8[>..] {
    let digits: const u8[:] = "0123456789abcdef";
    var out: u8[>..] = [];
    for b in bytes { out.push(digits[b >> 4]); out.push(digits[b & 15]); }
    return out;
}
fn unhex(text: const u8[:]) -> u8[>..] {
    var bytes: u8[>..] = [];
    if text == "-" { return bytes; }
    var i: i64 = 0;
    while i < text.len { let value, ok = parse_int(text[i..i + 2], 16); assert(ok); bytes.push(value as u8); i += 2; }
    return bytes;
}
fn uuid(v: wire::Uuid) -> u8[>..] { var s: u8[>..] = []; format(s, v.high, ":", v.low); return s; }
fn profile(p: login263::GameProfile&) -> u8[>..] {
    var s: u8[>..] = [];
    format(s, uuid(p.uuid), " ", hex(p.name), " ", p.properties.len);
    for v in p.properties { format(s, " | ", hex(v.name), " ", hex(v.value), " ", if v.has_signature { 1 } else { 0 }, " ", hex(v.signature)); }
    return s;
}
fn client(p: login263::Clientbound..&) -> u8[>..] {
    var s: u8[>..] = [];
    match p {
        Disconnect v => { format(s, "reason ", hex(v.reason_json)); },
        Hello v => { format(s, "hello ", hex(v.server_id), " ", hex(v.public_key), " ", hex(v.challenge), " ", if v.should_authenticate { 1 } else { 0 }); },
        LoginFinished v => { format(s, "finished ", profile(v.profile), " session ", uuid(v.session)); },
        Compression v => { format(s, "compression ", v.threshold); },
        CustomQuery v => { format(s, "query ", v.transaction, " ", hex(v.channel), " ", hex(v.data)); },
        CookieRequest v => { format(s, "cookie ", hex(v.key)); },
        Unknown v => { format(s, "unknown ", v.id, " ", hex(v.payload)); },
    }
    return s;
}
fn server(p: login263::Serverbound..&) -> u8[>..] {
    var s: u8[>..] = [];
    match p {
        Hello v => { format(s, "hello ", hex(v.name), " ", uuid(v.uuid)); },
        Key v => { format(s, "key ", hex(v.key), " ", hex(v.challenge)); },
        CustomQueryAnswer v => { format(s, "answer ", v.transaction, " ", if v.has_payload { 1 } else { 0 }, " ", hex(v.payload)); },
        LoginAcknowledged => { format(s, "ack"); },
        CookieResponse v => { format(s, "cookie ", hex(v.key), " ", if v.has_payload { 1 } else { 0 }, " ", hex(v.payload)); },
        Unknown v => { format(s, "unknown ", v.id, " ", hex(v.payload)); },
    }
    return s;
}
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let parts = split(line, ' ');
        let id, ok = parse_int(parts[1]); assert(ok);
        let data = unhex(parts[2]);
        if parts[0] == "C" {
            let p, error = login263::decode_clientbound(id as i32, data);
            if !wire::is_ok(error) { print("ERR ", wire::error_message(error)); }
            else { let encoded = login263::encode_clientbound(p); print("CLIENT ", login263::client_name(p), " ", client(p), " ", hex(encoded)); }
        } else {
            let p, error = login263::decode_serverbound(id as i32, data);
            if !wire::is_ok(error) { print("ERR ", wire::error_message(error)); }
            else { let encoded = login263::encode_serverbound(p); print("SERVER ", login263::server_name(p), " ", server(p), " ", hex(encoded)); }
        }
    }
}
