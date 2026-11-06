import std;
import net.packets;
fn main() {
    let p = packets263::Intention { host: "localhost", intent: packets263::LOGIN };
    let body = packets263::encode_intention(p);
    let expected: u8[16] = [0, 0x89, 6, 9, 'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't', 0x63, 0xdd, 2];
    assert(body == expected);
    let back, error = packets263::decode_intention(0, body[1..]);
    assert(wire::is_ok(error) && back.protocol == 777 && back.port == 25565 && back.host == "localhost" && back.intent == 2);
    let ping = packets263::StatusPacket { id: 1, time: -5 };
    let ping_body = packets263::encode_status(ping);
    assert(ping_body.len == 9 && ping_body[0] == 1 && ping_body[8] == 251);
    let pong, result = packets263::decode_status(1, ping_body[1..], false);
    assert(wire::is_ok(result) && pong.time == -5);
    let extra: u8[1] = [0];
    let bad, failure = packets263::decode_status(0, extra, true);
    assert(!wire::is_ok(failure));
    let unknown, ok = packets263::decode_status(55, extra, false);
    assert(wire::is_ok(ok) && !unknown.known && unknown.payload == extra);
    let again = packets263::encode_status(unknown);
    assert(again.len == 2 && again[0] == 55 && again[1] == 0);
    print("26.3 handshake/status: protocol 777, exact intention bytes, ping/pong, unknown payload and trailing-byte rejection passed.");
}
