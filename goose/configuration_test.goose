import std;
import net.configuration;
fn main() {
    let information = configuration263::default_information(12);
    let packet: configuration263::Serverbound.. = configuration263::Serverbound.ClientInformation { value: copy(information) };
    let bytes = configuration263::encode_serverbound(packet);
    let expected: u8[15] = [0,5,101,110,95,117,115,12,0,1,127,1,0,1,0];
    assert(bytes == expected);
    let back, e = configuration263::decode_serverbound(0, bytes[1..]); assert(wire::is_ok(e));
    match back { ClientInformation v => { assert(v.value.main_hand == 1 && v.value.view_distance == 12); }, _ => { assert(false); }, }
    let none: configuration263::Serverbound.. = configuration263::Serverbound.CustomClickAction { value: configuration263::CustomClickAction {
        id: "test:x", payload: configuration263::OptionalData {} } };
    let encoded = configuration263::encode_serverbound(none);
    assert(encoded[encoded.len - 2] == 1 && encoded[encoded.len - 1] == 0);
    let invalid: u8[1] = [1];
    let extra, error = configuration263::decode_clientbound(3, invalid);
    match error { PacketTooLarge v => { assert(v.extra == 1 && !v.serverbound); }, _ => { assert(false); }, }
    print("Configuration: default client information, absent click payload and full-consumption policy passed.");
}
