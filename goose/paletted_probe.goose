// Development-only observable-state probe of the translated container.
import std;
import os;
import core.paletted;
fn flag(value: bool) -> i64 { if value { 1 } else { 0 } }
fn emit_error(error: paletted::PaletteError) {
    match error {
        Ok => { print("OK"); },
        BadPalette e => { print("PALETTE ", e.len); },
        BadStorage e => { print("STORAGE ", e.expected, " ", e.actual); },
        BadIndex e => { print("INDEX ", e.index, " ", e.palette); },
        BadBits e => { print("BITS ", e.declared, " ", e.expected); },
    }
}
fn dump(c: paletted::Container&) {
    let value, found = paletted::uniform(c);
    print("STATE ", c.bits, " ", paletted::palette_len(c), " ", flag(paletted::is_global(c)), " ", flag(found), " ", if found { value } else { 0 });
    let wire = paletted::to_wire(c);
    var line: u8[>..] = [];
    format(line, "WIRE ", wire.bits, " ", wire.palette.len);
    for v in wire.palette { format(line, " ", v); }
    format(line, " WORDS ", wire.words.len);
    for word in wire.words { format(line, " ", word as! i64); }
    print(line);
    let saved = paletted::pack(c);
    var packed_line: u8[>..] = [];
    format(packed_line, "PACK ", saved.bits, " ", flag(saved.has_data), " ", saved.palette.len);
    for v in saved.palette { format(packed_line, " ", v); }
    format(packed_line, " WORDS ", saved.data.len);
    for word in saved.data { format(packed_line, " ", word as! i64); }
    print(packed_line);
    var values: u32[>..] = [];
    var ids: u16[>..] = [];
    for i in paletted::entries(c.strategy) { values.push(0); ids.push(65535); }
    paletted::unpack(c, values);
    let has_ids = paletted::unpack_ids(c, ids);
    var out: u8[>..] = [];
    format(out, "VALUES");
    for i in values.len { assert(values[i] == paletted::get(c, i)); format(out, " ", values[i]); }
    print(out);
    var id_line: u8[>..] = [];
    format(id_line, "IDS ", flag(has_ids));
    if has_ids { for id in ids { format(id_line, " ", id); } }
    else { for id in ids { assert(id == 65535); } }
    print(id_line);
    for start in [0, 1, 15, 16, values.len - 16] {
        var run: u32[16] = [0; 16];
        paletted::get_run(c, start, run);
        for i in run.len { assert(run[i] == values[start + i]); }
    }
}
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    var c = paletted::new_container(paletted::blocks(16), 0);
    var error: paletted::PaletteError = paletted::PaletteError.Ok;
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let parts = split(line, ' ');
        fn number(i: i64) -> i64 { let n, ok = parse_int(parts[i]); assert(ok); return n; }
        fn strategy() -> paletted::Strategy { paletted::Strategy { kind: number(1) as u8, global_bits: number(2) as u8 } }
        let op = parts[0];
        if op == "N" { c = paletted::new_container(strategy(), number(3) as u32); print("OK"); }
        else if op == "V" {
            var values: u32[>..] = [];
            for i in 3..parts.len { values.push(number(i) as u32); }
            c = paletted::from_values(strategy(), values);
            print("OK");
        } else if op == "W" || op == "P" {
            let bits_or_count = number(3);
            let palette_count = if op == "W" { number(4) } else { bits_or_count };
            let word_count = number(5);
            var palette: u32[>..] = [];
            var words: u64[>..] = [];
            for i in 6..6 + palette_count { palette.push(number(i) as u32); }
            for i in 6 + palette_count..parts.len { words.push(number(i) as! u64); }
            assert(words.len == word_count);
            if op == "W" { c, error = paletted::from_wire(strategy(), bits_or_count as u8, palette, words); }
            else { c, error = paletted::from_packed(strategy(), palette, number(4) != 0, words); }
            emit_error(error);
        } else if op == "S" { print("SET ", paletted::set(c, number(1), number(2) as u32)); }
        else if op == "G" { print("GET ", paletted::get(c, number(1))); }
        else if op == "D" { dump(c); }
        else if op == "H" {
            let a = number(1) as u32; let b = number(2) as u32;
            let actual = paletted::has(c) { it == a || it == b };
            let maybe = paletted::maybe_has(c) { it == a || it == b };
            print("HAS ", flag(actual), " ", flag(maybe));
        } else if op == "J" {
            let m = number(1) as u32; let r = number(2) as u32;
            let actual = paletted::has(c) { it % m == r };
            let maybe = paletted::maybe_has(c) { it % m == r };
            print("HAS ", flag(actual), " ", flag(maybe));
        } else if op == "I" { print("UNIFORM ", flag(paletted::is_uniform(c, number(1) as u32))); }
        else if op == "E" {
            var other = paletted::new_container(strategy(), number(3) as u32);
            print("EQUAL ", flag(paletted::equal(c, other)), " ", flag(paletted::same_storage(c, other)));
        } else if op == "R" {
            let saved = paletted::pack(c);
            var restored, result = paletted::from_packed(c.strategy, saved.palette, saved.has_data, saved.data);
            assert(paletted::is_ok(result) && paletted::equal(c, restored));
            print("RESTORE ", flag(paletted::same_storage(c, restored)));
            c = copy(restored);
        } else { abort("Unknown palette probe command"); }
    }
}
