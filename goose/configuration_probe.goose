import std;
import os;
import net.configuration;
fn hex(data: const u8[:]) -> u8[>..] {
    let digits: const u8[:] = "0123456789abcdef"; var out: u8[>..] = [];
    for b in data { out.push(digits[b >> 4]); out.push(digits[b & 15]); } return out;
}
fn main() {
    var input: u8[>..] = []; read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; } let p = split(line, ' ');
        let id, ok = parse_int(p[1]); assert(ok); var data: u8[>..] = [];
        if p[2] != "-" { var i: i64 = 0; while i < p[2].len { let b, valid = parse_int(p[2][i..i + 2],16); assert(valid); data.push(b as! u8); i += 2; } }
        if p[0] == "C" {
            let packet, error = configuration263::decode_clientbound(id as! i32, data);
            if !wire::is_ok(error) { let msg = wire::error_message(error); print("ERR ", msg); }
            else { let encoded = configuration263::encode_clientbound(packet); let text = hex(encoded); print("OK ", text); }
        } else {
            let packet, error = configuration263::decode_serverbound(id as! i32, data);
            if !wire::is_ok(error) { let msg = wire::error_message(error); print("ERR ", msg); }
            else { let encoded = configuration263::encode_serverbound(packet); let text = hex(encoded); print("OK ", text); }
        }
    }
}
