// Development-only raw-byte driver for the crypto subset.
import std;
import os;
import net.crypto;
fn unhex(text: const u8[:]) -> u8[>..] {
    var out: u8[>..] = [];
    if text == "-" { return out; }
    assert(text.len % 2 == 0);
    var i: i64 = 0;
    while i < text.len {
        let b, ok = parse_int(text[i..i + 2], 16); assert(ok);
        out.push(b as! u8); i += 2;
    }
    return out;
}
fn hex(data: const u8[:]) -> u8[>..] {
    let digits: const u8[:] = "0123456789abcdef";
    var out: u8[>..] = [];
    for b in data { out.push(digits[b >> 4]); out.push(digits[b & 15]); }
    return out;
}
fn main() {
    var input: u8[>..] = []; read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let p = split(line, ' ');
        if p[0] == "M" {
            let data = unhex(p[1]); let digest = cryptography::md5(data);
            let text = hex(digest); print(text);
        } else if p[0] == "U" {
            let name = unhex(p[1]);
            let units, error = wire::utf16_units(name);
            if !wire::is_ok(error) { print("INVALID"); }
            else { let text = cryptography::uuid_string(cryptography::offline_uuid(name)); print(text); }
        } else {
            let key = unhex(p[1]); let iv = unhex(p[2]);
            let chunk, ok = parse_int(p[3]); assert(ok && chunk > 0);
            var data = unhex(p[4]);
            var c = cryptography::new_cipher(key, iv); assert(c.handle != 0);
            var at: i64 = 0;
            while at < data.len {
                let end = min(at + chunk, data.len);
                if p[0] == "E" { assert(cryptography::encrypt(c, data[at..end])); }
                else { assert(p[0] == "D" && cryptography::decrypt(c, data[at..end])); }
                at = end;
            }
            let output = hex(data); let state = hex(c.register);
            print(output, ":", state);
            cryptography::close(c);
        }
    }
}
