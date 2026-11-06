import std;
import net.login;
import net.frame;
fn receive_body(body: const u8[:]) -> login263::Clientbound.. {
    var r = wire::reader(body);
    let id, id_error = wire::read_varint(r);
    assert(wire::is_ok(id_error));
    let packet, error = login263::decode_clientbound(id, wire::rest(r));
    assert(wire::is_ok(error));
    return copy(packet);
}
fn receive_frame(frame: const u8[:], threshold: i64, inflater: framing::Inflater&) -> login263::Clientbound.. {
    let h, header_error = framing::header(frame);
    assert(framing::is_ok(header_error) && h.complete && h.start + h.len == frame.len);
    let result, body_error = framing::body(frame[h.start..], threshold, inflater);
    assert(framing::is_ok(body_error));
    if result.plain { return receive_body(frame[h.start + result.start..]); }
    return receive_body(result.data);
}
fn main() {
    let hello: login263::Serverbound.. = login263::Serverbound.Hello { name: "Xylar", uuid: wire::Uuid { high: 1, low: 2 } };
    let raw = login263::encode_serverbound(hello);
    let packet, error = login263::decode_serverbound(0, raw[1..]);
    assert(wire::is_ok(error));
    match packet {
        Hello v => { assert(v.name == "Xylar" && v.uuid.high == 1 && v.uuid.low == 2); },
        _ => { abort("Expected login hello"); },
    }
    let properties: login263::Property[] = [
        login263::Property { name: "textures", value: "e30=" },
        login263::Property { name: "signed", value: "abc", has_signature: true, signature: "sig" },
    ];
    let profile = login263::GameProfile { uuid: wire::Uuid { high: 3, low: 4 }, name: "Xylar", properties: properties[..] };
    let finished: login263::Clientbound.. = login263::Clientbound.LoginFinished { profile: copy(profile), session: wire::Uuid { high: 5, low: 6 } };
    let encoded = login263::encode_clientbound(finished);
    let result, result_error = login263::decode_clientbound(2, encoded[1..]);
    assert(wire::is_ok(result_error));
    match result {
        LoginFinished v => {
            assert(v.session.high == 5 && v.session.low == 6);
            assert(v.profile.uuid == profile.uuid && v.profile.name == "Xylar" && v.profile.properties.len == 2);
            var index: i64 = 0;
            for property in v.profile.properties {
                if index == 0 { assert(!property.has_signature && property.name == "textures"); }
                else { assert(property.has_signature && property.signature == "sig"); }
                index += 1;
            }
        },
        _ => { abort("Expected finished profile"); },
    }
    let false_query: u8[3] = [3, 0, 9];
    let rejected, rejected_error = login263::decode_serverbound(2, false_query);
    assert(!wire::is_ok(rejected_error) && wire::error_message(rejected_error) == "packet 2 (Serverbound) was larger than expected: 1 extra bytes");
    let true_query: u8[2] = [3, 255];
    let query, query_error = login263::decode_serverbound(2, true_query);
    assert(wire::is_ok(query_error));
    match query {
        CustomQueryAnswer v => { assert(v.transaction == 3 && v.has_payload && v.payload.len == 0); },
        _ => { abort("Expected custom query answer"); },
    }
    let canonical = login263::encode_serverbound(query);
    let golden: u8[3] = [2, 3, 1];
    assert(canonical == golden);
    let unknown, unknown_error = login263::decode_clientbound(99, "raw");
    assert(wire::is_ok(unknown_error));
    let unknown_body = login263::encode_clientbound(unknown);
    assert(unknown_body[0] == 99 && unknown_body[1..] == "raw");
    // Exercise the actual codec boundary: compression is negotiated using the
    // previous framing mode; subsequent login packets use the new threshold.
    var inflater = framing::new_inflater();
    let compression: login263::Clientbound.. = login263::Clientbound.Compression { threshold: 256 };
    let compression_body = login263::encode_clientbound(compression);
    let negotiation, negotiation_error = framing::encode(compression_body, -1);
    assert(framing::is_ok(negotiation_error));
    let negotiation_packet = receive_frame(negotiation, -1, inflater);
    var threshold: i64 = -1;
    match negotiation_packet {
        Compression v => { threshold = v.threshold; },
        _ => { abort("Expected compression negotiation"); },
    }
    assert(threshold == 256);
    var query_data: u8[>..] = [];
    query_data.resize(1000, 7);
    let large_query: login263::Clientbound.. = login263::Clientbound.CustomQuery { transaction: 19, channel: "test:login", data: query_data[..] };
    let query_body = login263::encode_clientbound(large_query);
    let compressed, compress_error = framing::encode(query_body, threshold);
    assert(framing::is_ok(compress_error) && compressed.len < query_body.len);
    let received_query = receive_frame(compressed, threshold, inflater);
    match received_query {
        CustomQuery v => { assert(v.transaction == 19 && v.channel == "test:login" && v.data == query_data); },
        _ => { abort("Expected compressed login query"); },
    }
    let final_frame, final_error = framing::encode(encoded, threshold);
    assert(framing::is_ok(final_error));
    let final_profile = receive_frame(final_frame, threshold, inflater);
    match final_profile {
        LoginFinished v => { assert(v.profile.name == "Xylar" && v.session.high == 5 && v.session.low == 6); },
        _ => { abort("Expected login profile below compression threshold"); },
    }
    framing::close(inflater);
    print("Login 26.3: names, UUIDs, signed profile properties, session UUID, optional payloads and unknown packets passed.");
}
