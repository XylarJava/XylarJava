// Golden protocol 777 primitives, independent known wire bytes.
import std;
import net.buf;
fn main() {
    for n in [0, 1, 127, 128, 255, 25565, 2097151, 2147483647, -1, -2147483648] {
        var data: u8[>..] = [];
        wire::write_varint(data, n as i32);
        assert(data.len == wire::varint_size(n as i32));
        var r = wire::reader(data);
        let value, error = wire::read_varint(r);
        assert(wire::is_ok(error) && value == n && wire::remaining(r) == 0);
    }
    let bad: u8[6] = [255; 6];
    var invalid = wire::reader(bad);
    let ignored, error = wire::read_varint(invalid);
    assert(!wire::is_ok(error) && invalid.pos == 5 && wire::error_message(error) == "VarInt too big");
    var longs: u8[>..] = [];
    wire::write_varlong(longs, -9223372036854775808);
    assert(longs.len == 10 && longs[9] == 1);
    var lr = wire::reader(longs);
    let minimum, long_error = wire::read_varlong(lr);
    assert(wire::is_ok(long_error) && minimum == -9223372036854775808);
    var text: u8[>..] = [];
    wire::write_string(text, "😀😀😀😀😀😀😀😀");
    var ok_reader = wire::reader(text);
    let ok_text, ok_error = wire::read_string(ok_reader, 16);
    assert(wire::is_ok(ok_error) && ok_text.len == 32);
    var short_reader = wire::reader(text);
    let failed_text, short_error = wire::read_string(short_reader, 15);
    assert(!wire::is_ok(short_error) && wire::error_message(short_error) == "string longer than 15 chars");
    let short: u8[1] = [0xff];
    var r = wire::reader(short);
    let unavailable, short_int = wire::read_i32(r);
    assert(!wire::is_ok(short_int) && r.pos == 0);
    let partial_varint, partial = wire::read_varint(r);
    assert(!wire::is_ok(partial) && r.pos == 1);
    var set: u8[>..] = [0, 0, 0, 0];
    wire::set_bit(set, 3); wire::set_bit(set, 9);
    var encoded: u8[>..] = [];
    wire::write_bitset(encoded, set);
    assert(encoded.len == 3 && encoded[0] == 2 && encoded[1] == 8 && encoded[2] == 2);
    assert(wire::bit(set, 3) && wire::bit(set, 9) && !wire::bit(set, 100));
    print("Wire primitives: signed VarInts, cursor errors, UTF-16 string limits and BitSet passed.");
}
