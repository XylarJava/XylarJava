import std;
import os;
import data.nbt;
fn unhex(s: const u8[:]) -> u8[>..] {
    var out: u8[>..] = []; if s == "-" { return out; }
    var i: i64 = 0; while i < s.len { let n, ok = parse_int(s[i..i + 2], 16); assert(ok); out.push(n as! u8); i += 2; } return out;
}
fn hex(data: const u8[:]) -> u8[>..] {
    let digits: const u8[:] = "0123456789abcdef"; var out: u8[>..] = [];
    for b in data { out.push(digits[b >> 4]); out.push(digits[b & 15]); } return out;
}
fn main() {
    var input: u8[>..] = []; read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let p = split(line, ' ');
        let mb, a = parse_int(p[1]); let mn, b = parse_int(p[2]); let md, c = parse_int(p[3]); assert(a && b && c);
        let limits = nbt::Limits { max_bytes: mb, max_nodes: mn, max_depth: md };
        let data = unhex(p[4]);
        var doc, consumed, error = nbt::read(data, limits, p[0] == "N");
        if !nbt::is_ok(error) { let message = nbt::error_message(error); print("ERR ", message); continue; }
        let bytes, output_error = nbt::write(doc, limits, p[0] == "N");
        if !nbt::is_ok(output_error) { let message = nbt::error_message(output_error); print("WRITE ", message); continue; }
        let text = hex(bytes); print("OK ", consumed, " ", text);
    }
}
