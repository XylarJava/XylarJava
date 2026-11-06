import std;
import core.text;
fn accepted(text: u8[:], expected: i32) {
    let value, ok = java_text::parse_java_integer(text);
    assert(ok && value == expected);
}
fn main() {
    accepted("0", 0);
    accepted("-0", 0);
    accepted("+2147483647", 2147483647);
    accepted("-2147483648", -2147483648);
    accepted("١٢٣", 123);
    accepted("１２３", 123);
    accepted("+१२३", 123);
    accepted("1٢３", 123);
    for text in ["", "-", "+", "2147483648", "-2147483649", " 1", "1 ", "0x10", "1.0", "++1", "١٢x", "𝟙"] {
        let value, ok = java_text::parse_java_integer(text);
        assert(!ok);
    }
    let overlong: u8[2] = [0xc0, 0xb1];
    let value, ok = java_text::parse_java_integer(overlong);
    assert(!ok);
    for zero in java_text::ZEROES {
        var text: u8[>..] = [];
        push_utf8(text, zero + 4);
        push_utf8(text, zero + 2);
        accepted(text, 42);
    }
    print("Java integer parser: signed limits, BMP digit runs, mixed scripts and invalid UTF-8 passed.");
}
