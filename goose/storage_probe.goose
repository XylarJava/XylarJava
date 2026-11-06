// Development-only probe of the actual Goose port; no gameplay dependencies.
import std;
import os;
import core.storage;

fn emit_result(value: u32, error: packed::StorageError) {
    match error {
        Ok => { print("OK ", value); },
        InvalidBits e => { print("BITS ", e.bits); },
        InvalidLength e => { print("LENGTH ", e.expected, " ", e.actual); },
        OutOfBounds e => { print("BOUNDS ", e.index, " ", e.len); },
        ValueTooLarge e => { print("VALUE ", e.value); },
    }
}
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    var storage = packed::PackedStorage {};
    var error: packed::StorageError = packed::StorageError.Ok;
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let parts = split(line, ' ');
        fn number(index: i64) -> i64 {
            let value, ok = parse_int(parts[index]);
            assert(ok);
            return value;
        }
        let op = parts[0];
        if op == "N" {
            storage, error = packed::new_storage(number(1) as u8, number(2), number(3) != 0);
            emit_result(0, error);
        } else if op == "F" {
            var words: u64[>..] = [];
            for i in 4..parts.len { words.push(number(i) as! u64); }
            storage, error = packed::from_words(number(1) as u8, number(2), words, number(3) != 0);
            emit_result(0, error);
        } else if op == "G" {
            let value, result = packed::get(storage, number(1));
            emit_result(value, result);
        } else if op == "S" {
            let value, result = packed::set(storage, number(1), number(2) as u32);
            emit_result(value, result);
        } else if op == "W" {
            print("OK ", packed::write(storage, number(1), number(2) as u32));
        } else if op == "L" {
            emit_result(0, packed::fill_storage(storage, number(1) as u32));
        } else if op == "A" {
            var old_sum: u64 = 0;
            for i in storage.len {
                let value = (((i * 7 + number(1)) as u64) & storage.mask) as u32;
                let old = packed::write(storage, i, value);
                old_sum += old;
                let checked, result = packed::get(storage, i);
                assert(packed::is_ok(result) && checked == packed::read(storage, i));
            }
            print("A ", old_sum);
        } else if op == "D" {
            var output: u8[>..] = [];
            format(output, "WORDS");
            for word in storage.words { format(output, " ", word as! i64); }
            format(output, " VALUES");
            let values = packed::unpack(storage);
            var into: u32[>..] = [];
            for i in storage.len + 3 { into.push(4294967295); }
            packed::unpack_into(storage, into);
            for i in values.len { assert(values[i] == into[i]); format(output, " ", values[i]); }
            for i in storage.len..into.len { assert(into[i] == 4294967295); }
            assert(packed::word_bytes(storage) == storage.words.len * 8);
            print(output);
        } else { abort("Unknown probe operation"); }
    }
}
