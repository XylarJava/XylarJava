import std;
import os;
import net.frame;
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
fn digest(bytes: const u8[:]) -> u64 {
    var h: u64 = 0xcbf29ce484222325;
    for byte in bytes { h = (h ^ byte) * 0x100000001b3; }
    return h;
}
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    var inflater = framing::new_inflater();
    assert(inflater.handle != 0);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let p = split(line, ' ');
        fn number(i: i64) -> i64 { let value, ok = parse_int(p[i]); assert(ok); return value; }
        if p[0] == "H" {
            let bytes = unhex(p[1]);
            let h, error = framing::header(bytes);
            if !framing::is_ok(error) { print("ERR ", framing::error_message(error)); }
            else if !h.complete { print("NONE"); }
            else { print("HEADER ", h.start, " ", h.len); }
        } else if p[0] == "P" {
            let bytes = unhex(p[1]); let out = framing::prefixed(bytes); print("PREFIX ", hex(out));
        } else if p[0] == "E" {
            let bytes = unhex(p[2]); let out, error = framing::encode(bytes, number(1));
            if !framing::is_ok(error) { print("ERR ", framing::error_message(error)); }
            else { print("ENC ", hex(out)); }
        } else if p[0] == "B" {
            let bytes = unhex(p[2]); let out, error = framing::body(bytes, number(1), inflater);
            if !framing::is_ok(error) { print("ERR ", framing::error_message(error)); }
            else if out.plain { print("PLAIN ", out.start, " ", hex(bytes[out.start..])); }
            else { print("INFLATED ", hex(out.data)); }
        } else if p[0] == "X" {
            var bytes: u8[>..] = [];
            let kind = number(3);
            for i in number(2) { bytes.push(if kind == 0 { (i % 7) as u8 } else { (((i as u64) * 0x9e3779b97f4a7c15) >> 56) as u8 }); }
            let out, error = framing::encode(bytes, number(1));
            if !framing::is_ok(error) { print("ERR ", framing::error_message(error)); }
            else {
                let h, he = framing::header(out); assert(framing::is_ok(he) && h.complete);
                let result, re = framing::body(out[h.start..h.start + h.len], number(1), inflater);
                if !framing::is_ok(re) { print("BODYERR ", out.len, " ", digest(out), " ", framing::error_message(re)); }
                else if result.plain { print("LARGE ", out.len, " ", digest(out), " ", digest(out[h.start + result.start..])); }
                else { print("LARGE ", out.len, " ", digest(out), " ", digest(result.data)); }
            }
        } else { abort("Unknown frame fixture"); }
    }
    framing::close(inflater);
}
