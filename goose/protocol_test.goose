import protocol;
import std;

fn golden(value: i32, expected: u8[:]) {
    var encoded: u8[>..] = [];
    write_varint(encoded, value);
    assert(encoded.len == expected.len);
    for i in expected.len { assert(encoded[i] == expected[i]); }
    let decoded, used, ok = read_varint(expected);
    assert(ok && decoded == value && used == expected.len);
    // Every proper prefix of a multi-byte value must request more data.
    for n in expected.len {
        let partial, consumed, success = read_varint(expected[0..n]);
        assert(!success && consumed == 0);
    }
}

fn main() {
    golden(0, [0]);
    golden(1, [1]);
    golden(127, [127]);
    golden(128, [128, 1]);
    golden(255, [255, 1]);
    golden(300, [172, 2]);
    golden(16383, [255, 127]);
    golden(16384, [128, 128, 1]);
    golden(2147483647, [255, 255, 255, 255, 7]);
    golden(-1, [255, 255, 255, 255, 15]);
    golden(-2147483648, [128, 128, 128, 128, 8]);
    let oversized: u8[6] = [128, 128, 128, 128, 128, 0];
    let bad, consumed, success = read_varint(oversized);
    assert(!success && consumed == 0);
    let trailing: u8[3] = [172, 2, 42];
    let value, used, ok = read_varint(trailing);
    assert(ok && value == 300 && used == 2);
    print("Goose protocol: golden vectors, incomplete input, oversized input and trailing bytes passed.");
}
