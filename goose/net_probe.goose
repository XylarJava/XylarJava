// Development-only protocol 777 comparison driver.
import std;
import os;
import net.buf;
import net.chunk;
import net.packets;
fn hex(bytes: const u8[:]) -> u8[>..] {
    let digits: const u8[:] = "0123456789abcdef";
    var out: u8[>..] = [];
    for b in bytes { out.push(digits[b >> 4]); out.push(digits[b & 15]); }
    return out;
}
fn unhex(text: const u8[:]) -> u8[>..] {
    var data: u8[>..] = [];
    if text == "-" { return data; }
    var i: i64 = 0;
    while i < text.len { let value, ok = parse_int(text[i..i + 2], 16); assert(ok); data.push(value as u8); i += 2; }
    return data;
}
fn describe(p: chunk_wire::Paletted&) -> u8[>..] {
    var out: u8[>..] = [];
    format(out, p.bits, " P ", p.palette.len);
    for value in p.palette { format(out, " ", value); }
    format(out, " D ", p.data.len);
    for word in p.data { format(out, " ", word); }
    format(out, " G");
    for index in [0, 11, 12, 13, 63, 64, 4095, 4096, 100000] { format(out, " ", chunk_wire::get(p, index)); }
    return out;
}
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let parts = split(line, ' ');
        fn n(i: i64) -> i64 { let value, ok = parse_int(parts[i]); assert(ok); return value; }
        let op = parts[0];
        if op == "R" {
            let bytes = unhex(parts[4]);
            var r = wire::reader(bytes);
            r.pos = n(3);
            var error: wire::Error = wire::Error.Ok;
            var value: u8[>..] = [];
            var output: u8[>..] = [];
            let code = n(1);
            if code == 0 { let v, e = wire::read_u8(r); error = e; format(value, v); wire::write_u8(output, v); }
            else if code == 1 { let v, e = wire::read_i8(r); error = e; format(value, v); wire::write_i8(output, v); }
            else if code == 2 { let v, e = wire::read_bool(r); error = e; format(value, if v { 1 } else { 0 }); wire::write_bool(output, v); }
            else if code == 3 { let v, e = wire::read_i16(r); error = e; format(value, v); wire::write_i16(output, v); }
            else if code == 4 { let v, e = wire::read_u16(r); error = e; format(value, v); wire::write_u16(output, v); }
            else if code == 5 { let v, e = wire::read_i32(r); error = e; format(value, v); wire::write_i32(output, v); }
            else if code == 6 { let v, e = wire::read_u32(r); error = e; format(value, v); wire::write_u32(output, v); }
            else if code == 7 { let v, e = wire::read_i64(r); error = e; format(value, v); wire::write_i64(output, v); }
            else if code == 8 { let v, e = wire::read_varint(r); error = e; format(value, v); wire::write_varint(output, v); }
            else if code == 9 { let v, e = wire::read_varlong(r); error = e; format(value, v); wire::write_varlong(output, v); }
            else if code == 10 { let v, e = wire::read_uuid(r); error = e; format(value, v.high as! i64, ":", v.low as! i64); wire::write_uuid(output, v); }
            else if code == 11 { let v, e = wire::read_string(r, n(2)); error = e; format(value, "B:", hex(v)); wire::write_string(output, v); }
            else if code == 12 || code == 16 {
                let maximum = if code == 16 { wire::remaining(r) } else { n(2) };
                let v, e = wire::read_byte_array(r, maximum); error = e; format(value, "B:", hex(v));
                if code == 16 { wire::write_bitset(output, v); } else { wire::write_byte_array(output, v); }
            } else if code == 13 || code == 14 {
                if code == 13 {
                    let v, e = wire::read_fixed_longs(r, n(2)); error = e; format(value, "L:");
                    for item in v { format(value, item, ","); wire::write_i64(output, item); }
                } else {
                    let v, e = wire::read_long_array(r); error = e; format(value, "L:");
                    for item in v { format(value, item, ","); } wire::write_long_array(output, v);
                }
            } else if code == 15 { let v, e = wire::read_count(r, n(2)); error = e; format(value, v); wire::write_varint(output, v as i32); }
            else if code == 17 { let v, e = wire::read_bytes(r, n(2)); error = e; format(value, "B:", hex(v)); output.append(v); }
            else if code == 18 { let v = wire::since(r, n(2)); format(value, "B:", hex(v)); output.append(v); }
            else if code == 19 { let v = wire::rest(r); format(value, "B:", hex(v)); output.append(v); }
            else { abort("Unknown primitive"); }
            if wire::is_ok(error) { print("OK ", r.pos, " ", value, " ", hex(output)); }
            else { print("ERR ", r.pos, " ", wire::error_message(error)); }
        } else if op == "H" {
            let bytes = unhex(parts[2]);
            let p, error = packets263::decode_intention(n(1) as i32, bytes);
            if !wire::is_ok(error) { print("ERR ", wire::error_message(error)); }
            else {
                let encoded = packets263::encode_intention(p);
                if p.known { print("H K ", p.protocol, " ", p.port, " ", p.intent, " ", hex(p.host), " ", hex(encoded)); }
                else { print("H U ", p.id, " ", hex(p.payload), " ", hex(encoded)); }
            }
        } else if op == "T" {
            let bytes = unhex(parts[3]);
            let p, error = packets263::decode_status(n(2) as i32, bytes, n(1) != 0);
            if !wire::is_ok(error) { print("ERR ", wire::error_message(error)); }
            else {
                let encoded = packets263::encode_status(p);
                if p.known { print("T K ", p.id, " ", p.time, " ", hex(p.json), " ", hex(encoded)); }
                else { print("T U ", p.id, " ", hex(p.payload), " ", hex(encoded)); }
            }
        } else if op == "V" {
            var values: i32[>..] = [];
            for i in 3..parts.len { values.push(n(i) as i32); }
            let p = chunk_wire::from_values(values, n(1) != 0, n(2) as u8);
            var output: u8[>..] = [];
            chunk_wire::write_paletted(p, output);
            print("PC ", describe(p), " ", hex(output));
        } else if op == "C" {
            let bytes = unhex(parts[3]);
            var r = wire::reader(bytes);
            let p, error = chunk_wire::read_paletted(r, n(1) != 0, n(2) as u8);
            if !wire::is_ok(error) { print("ERR ", r.pos, " ", wire::error_message(error)); }
            else {
                var output: u8[>..] = []; chunk_wire::write_paletted(p, output);
                print("PC ", r.pos, " ", describe(p), " ", hex(output));
            }
        } else if op == "S" {
            let bytes = unhex(parts[3]);
            var r = wire::reader(bytes);
            let s, error = chunk_wire::read_section(r, n(1) as u8, n(2) as u8);
            if !wire::is_ok(error) { print("ERR ", r.pos, " ", wire::error_message(error)); }
            else {
                var output: u8[>..] = []; chunk_wire::write_section(s, output);
                print("SC ", r.pos, " ", s.non_empty, " ", s.fluids, " | ", describe(s.states), " | ", describe(s.biomes), " ", hex(output));
            }
        } else { abort("Unknown protocol fixture command"); }
    }
}
