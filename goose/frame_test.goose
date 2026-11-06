import std;
import net.frame;
fn main() {
    let data: u8[4] = [0, 1, 2, 3];
    let encoded, error = framing::encode(data);
    let golden: u8[5] = [4, 0, 1, 2, 3];
    assert(framing::is_ok(error) && encoded == golden);
    let prefix, prefix_error = framing::header(encoded);
    assert(framing::is_ok(prefix_error) && prefix.complete && prefix.start == 1 && prefix.len == 4);
    let incomplete, incomplete_error = framing::header([128, 128]);
    assert(framing::is_ok(incomplete_error) && !incomplete.complete);
    let bad, bad_error = framing::header([128, 128, 128]);
    assert(!framing::is_ok(bad_error));
    let zero, zero_error = framing::header([128, 0]);
    assert(!framing::is_ok(zero_error));
    var inflater = framing::new_inflater();
    assert(inflater.handle != 0);
    for length in [256, 300, 5000, 70000] {
        var values: u8[>..] = [];
        for i in length { values.push(((i * 7) % 13) as u8); }
        let frame, encode_error = framing::contents(values, 256);
        assert(framing::is_ok(encode_error));
        let result, decode_error = framing::body(frame, 256, inflater);
        assert(framing::is_ok(decode_error) && !result.plain && result.data == values);
        var malformed: u8[>..] = [];
        wire::write_varint(malformed, (length - 1) as i32);
        var reader = wire::reader(frame);
        let ignored, ignored_error = wire::read_varint(reader);
        malformed.append(frame[reader.pos..]);
        let wrong, wrong_error = framing::body(malformed, 0, inflater);
        assert(!framing::is_ok(wrong_error));
        let again, again_error = framing::body(frame, 256, inflater);
        assert(framing::is_ok(again_error) && again.data == values);
    }
    let wide_zero: u8[5] = [128, 0, 7, 1, 2];
    let plain, plain_error = framing::body(wide_zero, 256, inflater);
    assert(framing::is_ok(plain_error) && plain.plain && plain.start == 2);
    framing::close(inflater);
    framing::close(inflater);
    print("Framing: VarInt21, thresholds, zlib round trips, size errors and inflater reuse passed.");
}
