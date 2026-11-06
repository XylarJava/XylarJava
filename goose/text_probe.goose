import std;
import os;
import core.text;
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    let lines = split(input, '\n');
    // A terminating newline is framing, not an extra empty fixture.
    for line in lines[0..lines.len - 1] {
        assert(line.len % 2 == 0);
        var text: u8[>..] = [];
        var i: i64 = 0;
        while i < line.len {
            let byte, ok = parse_int(line[i..i + 2], 16);
            assert(ok);
            text.push(byte as u8);
            i += 2;
        }
        let value, ok = java_text::parse_java_integer(text);
        if ok { print("OK ", value); } else { print("NONE"); }
    }
}
