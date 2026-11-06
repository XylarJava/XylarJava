import std;
import os;
import net.chunk;
import net.play;
fn hex(data: const u8[:]) -> u8[>..] {
    let digits: const u8[:] = "0123456789abcdef"; var out: u8[>..] = [];
    for b in data { out.push(digits[b >> 4]); out.push(digits[b & 15]); } return out;
}
fn main() {
    var input: u8[>..] = []; read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; } let p = split(line, ' ');
        let count, ok = parse_int(p[1]); assert(ok); var data: u8[>..] = [];
        if p[2] != "-" { var i: i64 = 0; while i < p[2].len { let b, valid = parse_int(p[2][i..i + 2],16); assert(valid); data.push(b as! u8); i += 2; } }
        var r = wire::reader(data); var out: u8[>..] = [];
        if p[0] == "P" {
            let packet, e = play263::decode_clientbound(count as! i32, data);
            if !wire::is_ok(e) { let msg = wire::error_message(e); print("ERR ", msg); }
            else { let encoded = play263::encode_clientbound(packet); let text = hex(encoded); print("OK ", text); }
        } else if p[0] == "C" {
            let chunk, e = chunk_wire::read_chunk(r);
            if !wire::is_ok(e) { let msg = wire::error_message(e); print("ERR ", r.pos, " ", msg); continue; }
            chunk_wire::write_chunk(chunk, out); let text = hex(out); print("OK ", r.pos, " ", text);
            let sections, se = chunk_wire::sections(chunk, count, 16, 7);
            if !wire::is_ok(se) { let msg = wire::error_message(se); print("SE ", msg); }
            else { let bytes = chunk_wire::from_sections(sections); let text = hex(bytes); print("SC ", text); }
        } else {
            let light, e = chunk_wire::read_light(r);
            if !wire::is_ok(e) { let msg = wire::error_message(e); print("ERR ", r.pos, " ", msg); continue; }
            chunk_wire::write_light(light, out); let text = hex(out); print("OK ", r.pos, " ", text);
            let expanded = chunk_wire::layers(light, count);
            for layer in expanded {
                let sky = hex(layer.sky); let bl = hex(layer.block_light);
                print("L ", if layer.has_sky { 1 } else { 0 }, " ", sky, " ", if layer.has_block { 1 } else { 0 }, " ", bl);
            }
        }
    }
}
