import std;
import net.connection;
fn main() {
    var c = transport::new_detached();
    let data: u8[4] = [0, 1, 2, 3];
    let frame, frame_error = framing::encode(data);
    assert(framing::is_ok(frame_error));
    for i in frame.len {
        transport::feed(c, frame[i..i + 1]);
        let packet, available, error = transport::buffered(c);
        assert(transport::is_ok(error));
        assert(available == (i + 1 == frame.len));
        if available { assert(packet.id == 0 && packet.payload == data[1..]); }
    }
    assert(c.received == 1 && c.bytes_in == frame.len);
    transport::set_compression(c, 64);
    let empty: u8[2] = [1, 0];
    transport::feed(c, empty);
    let packet, available, error = transport::buffered(c);
    assert(transport::is_ok(error) && !available && c.received == 1);
    var large: u8[>..] = [5];
    large.resize(200001, 3);
    let compressed, compress_error = framing::encode(large, 64);
    assert(framing::is_ok(compress_error));
    transport::feed(c, compressed);
    let big, ready, result = transport::buffered(c);
    assert(transport::is_ok(result) && ready && big.id == 5 && big.payload.len == 200000);
    assert(c.received == 2);
    transport::set_compression(c, -100);
    assert(c.threshold == -1);
    transport::close(c);
    transport::close(c);
    // Read-ahead crosses the encryption switch: decrypt the already buffered
    // ciphertext exactly once, then continue with the same cipher register.
    var writer = transport::new_detached();
    var reader = transport::new_detached();
    var stream: u8[>..] = [];
    let first_write = transport::frame_into(writer, data, stream);
    assert(transport::is_ok(first_write));
    let key: u8[16] = [7; 16];
    let encryption = transport::enable_encryption(writer, key);
    assert(transport::is_ok(encryption));
    let next_data: u8[3] = [5,4,3];
    let second_write = transport::frame_into(writer, next_data, stream);
    assert(transport::is_ok(second_write));
    let fed = transport::feed(reader, stream); assert(transport::is_ok(fed));
    let first_packet, first_ready, first_error = transport::buffered(reader);
    assert(transport::is_ok(first_error) && first_ready && first_packet.id == 0);
    let encrypted_reader = transport::enable_encryption(reader, key);
    assert(transport::is_ok(encrypted_reader));
    let second_packet, second_ready, second_error = transport::buffered(reader);
    assert(transport::is_ok(second_error) && second_ready && second_packet.id == 5 && second_packet.payload == next_data[1..]);
    assert(writer.sent == 2 && writer.bytes_out == stream.len && reader.received == 2);
    transport::close(reader); transport::close(writer);
    print("Connection: fragmented reads, empty-frame skip, compression, counters and cleanup passed.");
}
