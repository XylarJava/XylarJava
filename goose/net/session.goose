// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Current-version ping and plain offline login flow from client net.rs.
// MSA session joining and gameplay remain pending. RSA/AES for
// unauthenticated encrypted servers uses an explicit OpenSSL adaptation.
// Full notice: ../licenses/upstream-MIT.txt.
import std;
import json;
import net.connection;
import net.packets;
import net.login;
import net.configuration;
import net.play;
import net.player_sync;
import net.column;
namespace session263;
struct Ping { json: u8[] = [], latency_ms: i64 = 0 }
fn intention(c: transport::Connection&, host: const u8[:], port: u16, intent: i32) -> transport::Error {
    let packet = packets263::Intention { protocol: 777, host: host, port: port, intent: intent };
    let data = packets263::encode_intention(packet);
    return transport::send(c, data);
}
fn ping(c: transport::Connection&, host: const u8[:], port: u16) -> Ping, transport::Error {
    let handshake_error = intention(c, host, port, packets263::STATUS);
    if !transport::is_ok(handshake_error) { return Ping {}, copy(handshake_error); }
    let request = packets263::StatusPacket {};
    let request_data = packets263::encode_status(request);
    let send_error = transport::send(c, request_data);
    if !transport::is_ok(send_error) { return Ping {}, copy(send_error); }
    let response, response_error = transport::recv(c);
    if !transport::is_ok(response_error) { return Ping {}, copy(response_error); }
    let status, status_error = packets263::decode_status(response.id, response.payload, false);
    if !wire::is_ok(status_error) { return Ping {}, transport::failure(wire::error_message(status_error)); }
    if !status.known || status.id != 0 { return Ping {}, transport::failure("unexpected status reply"); }
    var nodes: json::Node[>..] = [];
    var decoded: u8[>..] = [];
    let document, json_error = json::parse(status.json, nodes, decoded);
    if !document { return Ping {}, transport::failure("Invalid status JSON"); }
    let start = clock_ns();
    let ping = packets263::StatusPacket { id: 1, time: 1 };
    let ping_data = packets263::encode_status(ping);
    let ping_error = transport::send(c, ping_data);
    if !transport::is_ok(ping_error) { return Ping {}, copy(ping_error); }
    let pong, pong_error = transport::recv(c);
    if !transport::is_ok(pong_error) { return Ping {}, copy(pong_error); }
    // The reference ping function waits for one reply without inspecting it.
    return Ping { json: status.json[..], latency_ms: (clock_ns() - start) / 1000000 }, transport::Error {};
}
fn offline_login(c: transport::Connection&, host: const u8[:], port: u16, name: const u8[:]) -> login263::GameProfile, transport::Error {
    let units, name_error = wire::utf16_units(name);
    if !wire::is_ok(name_error) || units > 16 { return login263::GameProfile {}, transport::failure("Invalid player name"); }
    let handshake_error = intention(c, host, port, packets263::LOGIN);
    if !transport::is_ok(handshake_error) { return login263::GameProfile {}, copy(handshake_error); }
    let hello: login263::Serverbound.. = login263::Serverbound.Hello { name: name, uuid: cryptography::offline_uuid(name) };
    let hello_body = login263::encode_serverbound(hello);
    let hello_error = transport::send(c, hello_body);
    if !transport::is_ok(hello_error) { return login263::GameProfile {}, copy(hello_error); }
    loop {
        let received, receive_error = transport::recv(c);
        if !transport::is_ok(receive_error) { return login263::GameProfile {}, copy(receive_error); }
        let packet, decode_error = login263::decode_clientbound(received.id, received.payload);
        if !wire::is_ok(decode_error) { return login263::GameProfile {}, transport::failure(wire::error_message(decode_error)); }
        match packet {
            Compression v => { transport::set_compression(c, v.threshold); },
            LoginFinished v => {
                let ack: login263::Serverbound.. = login263::Serverbound.LoginAcknowledged;
                let body = login263::encode_serverbound(ack);
                let error = transport::send(c, body);
                if !transport::is_ok(error) { return login263::GameProfile {}, copy(error); }
                return copy(v.profile), transport::Error {};
            },
            Disconnect v => { return login263::GameProfile {}, transport::failure(v.reason_json); },
            Hello v => {
                if v.should_authenticate { return login263::GameProfile {}, transport::failure("Microsoft account authentication is required for this server"); }
                var key = cryptography::public_key(v.public_key);
                if key.handle == 0 { return login263::GameProfile {}, transport::failure("Malformed RSA public key"); }
                var secret: u8[16] = [0; 16];
                if !cryptography::secure_random(secret) {
                    cryptography::close(key);
                    return login263::GameProfile {}, transport::failure("No secure random source");
                }
                let encrypted_key, key_ok = cryptography::encrypt(key, secret);
                let encrypted_challenge, challenge_ok = cryptography::encrypt(key, v.challenge);
                cryptography::close(key);
                if !key_ok || !challenge_ok { return login263::GameProfile {}, transport::failure("RSA encryption failed"); }
                let answer: login263::Serverbound.. = login263::Serverbound.Key { key: encrypted_key[..], challenge: encrypted_challenge[..] };
                let body = login263::encode_serverbound(answer);
                let error = transport::send(c, body);
                if !transport::is_ok(error) { return login263::GameProfile {}, copy(error); }
                let encryption_error = transport::enable_encryption(c, secret);
                if !transport::is_ok(encryption_error) { return login263::GameProfile {}, copy(encryption_error); }
            },
            CustomQuery v => {
                let answer: login263::Serverbound.. = login263::Serverbound.CustomQueryAnswer { transaction: v.transaction, has_payload: false, payload: [] };
                let body = login263::encode_serverbound(answer);
                let error = transport::send(c, body);
                if !transport::is_ok(error) { return login263::GameProfile {}, copy(error); }
            },
            CookieRequest v => {
                // A plain join carries no transferred cookies.
                let answer: login263::Serverbound.. = login263::Serverbound.CookieResponse { key: v.key[..], has_payload: false, payload: [] };
                let body = login263::encode_serverbound(answer);
                let error = transport::send(c, body);
                if !transport::is_ok(error) { return login263::GameProfile {}, copy(error); }
            },
            Unknown v => {},
        }
    }
}

// Configuration data is retained for the future play-state/world binder. No
// prepared known packs are claimed, so the server must send registry values.
struct Cookie { key: u8[] = [], data: u8[] = [] }
struct Configured { registries: configuration263::RegistryData[] = [], tags: configuration263::RegistryTags[] = [],
    features: configuration263::Text[] = [], cookies: Cookie[] = [], dimensions: registry263::Layout[] = [], biome_bits: u8 = 0 }
fn configuration_reply(c: transport::Connection&, p: configuration263::Serverbound..&) -> transport::Error {
    let bytes = configuration263::encode_serverbound(p); return transport::send(c, bytes);
}
fn configure(c: transport::Connection&, view_distance: i8 = 8) -> Configured, transport::Error {
    var brand: u8[>..] = []; wire::write_string(brand, "xylar-goose");
    let brand_packet: configuration263::Serverbound.. = configuration263::Serverbound.CustomPayload {
        value: configuration263::CustomPayload { channel: "minecraft:brand", data: brand[..] } };
    let brand_error = configuration_reply(c, brand_packet); if !transport::is_ok(brand_error) { return Configured {}, copy(brand_error); }
    let information: configuration263::Serverbound.. = configuration263::Serverbound.ClientInformation { value: configuration263::default_information(view_distance) };
    let information_error = configuration_reply(c, information); if !transport::is_ok(information_error) { return Configured {}, copy(information_error); }
    var registries: configuration263::RegistryData[>..] = [];
    var tags: configuration263::RegistryTags[>..] = [];
    var features: configuration263::Text[>..] = [];
    var cookies: Cookie[>..] = [];
    loop {
        let received, receive_error = transport::recv(c);
        if !transport::is_ok(receive_error) { return Configured {}, copy(receive_error); }
        let packet, error = configuration263::decode_clientbound(received.id, received.payload);
        if !wire::is_ok(error) { return Configured {}, transport::failure(wire::error_message(error)); }
        // Every reply uses the connection's negotiated compression/cipher state.
        match packet {
            KeepAlive v => {
                let reply: configuration263::Serverbound.. = configuration263::Serverbound.KeepAlive { value: copy(v.value) };
                let e = configuration_reply(c, reply); if !transport::is_ok(e) { return Configured {}, copy(e); }
            },
            Ping v => {
                let reply: configuration263::Serverbound.. = configuration263::Serverbound.Pong { value: copy(v.value) };
                let e = configuration_reply(c, reply); if !transport::is_ok(e) { return Configured {}, copy(e); }
            },
            SelectKnownPacks v => {
                let reply: configuration263::Serverbound.. = configuration263::Serverbound.SelectKnownPacks { value: configuration263::SelectKnownPacks {} };
                let e = configuration_reply(c, reply); if !transport::is_ok(e) { return Configured {}, copy(e); }
            },
            RegistryData v => { registries.push(copy(v.value)); },
            UpdateTags v => { for registry in v.value.registries { tags.push(copy(registry)); } },
            UpdateEnabledFeatures v => { features.clear(); for feature in v.value.features { features.push(copy(feature)); } },
            StoreCookie v => { cookies.push(Cookie { key: v.value.key[..], data: v.value.payload[..] }); },
            CookieRequest v => {
                var found = false; var value: u8[>..] = [];
                for cookie in cookies { if cookie.key == v.value.key { found = true; value.clear(); value.append(cookie.data); } }
                let reply: configuration263::Serverbound.. = configuration263::Serverbound.CookieResponse { value: configuration263::CookieResponse {
                    key: v.value.key[..], payload: configuration263::OptionalData { present: found, data: value[..] } } };
                let e = configuration_reply(c, reply); if !transport::is_ok(e) { return Configured {}, copy(e); }
            },
            ResourcePackPush v => {
                let reply: configuration263::Serverbound.. = configuration263::Serverbound.ResourcePack { value: configuration263::ResourcePackResponse { id: v.value.id, action: 1 } };
                let e = configuration_reply(c, reply); if !transport::is_ok(e) { return Configured {}, copy(e); }
            },
            FinishConfiguration => {
                let reply: configuration263::Serverbound.. = configuration263::Serverbound.FinishConfiguration;
                let e = configuration_reply(c, reply); if !transport::is_ok(e) { return Configured {}, copy(e); }
                let dimensions,de=registry263::dimensions(registries);
                if !wire::is_ok(de) {return Configured {},transport::failure(wire::error_message(de));}
                let biome_bits=paletted::ceil_log2(registry263::registry_len(registries,"minecraft:worldgen/biome"));
                return Configured { registries: registries[..], tags: tags[..], features: features[..], cookies: cookies[..], dimensions: dimensions[..], biome_bits: biome_bits }, transport::Error {};
            },
            CodeOfConduct v => { return Configured {}, transport::failure("Server code of conduct requires user acceptance"); },
            ShowDialog v => { return Configured {}, transport::failure("Server requires an interactive dialog"); },
            Transfer v => { return Configured {}, transport::failure("Server transfer is not supported yet"); },
            Disconnect v => { return Configured {}, transport::failure("Server disconnected during configuration"); },
            _ => {},
        }
    }
}

// Bounded terrain diagnostic, not the gameplay loop. The caller owns the
// returned chunk. Login and teleport pose/velocity are tracked; world binding
// and an input/physics/render tick loop remain pending.
fn receive_terrain(c: transport::Connection&, player: player_sync263::State&, dimensions: registry263::Layout[:] = [], maximum_packets: i64 = 4096) -> play263::Clientbound.., transport::Error {
    for i in maximum_packets {
        let received, e = transport::recv(c);
        if !transport::is_ok(e) { return play263::Clientbound.BundleDelimiter, copy(e); }
        let packet, error = play263::decode_clientbound(received.id, received.payload);
        if !wire::is_ok(error) { return play263::Clientbound.BundleDelimiter, transport::failure(wire::error_message(error)); }
        var reply: u8[>..] = [];
        match packet {
            Login p => { player_sync263::login(player,p.value); player.layout=registry263::dimension(dimensions,p.value.spawn.dimension_type); },
            PlayerPosition p => {
                let ack = player_sync263::teleport(player,p.value);
                let e = transport::send(c,ack);
                if !transport::is_ok(e) { return play263::Clientbound.BundleDelimiter, copy(e); }
            },
            Respawn p => { return play263::Clientbound.BundleDelimiter, transport::failure("World replacement on respawn is not implemented yet"); },
            LevelChunkWithLight p => { return copy(packet), transport::Error {}; },
            KeepAlive p => { wire::write_varint(reply, 28); wire::write_i64(reply, p.id); },
            Ping p => { wire::write_varint(reply, 45); wire::write_i32(reply, p.id); },
            ChunkBatchFinished p => {
                // remote.rs advertises 64.0 chunks/tick; IEEE binary32 bytes.
                wire::write_varint(reply, 11); wire::write_u32(reply, 0x42800000);
            },
            Disconnect p => { return play263::Clientbound.BundleDelimiter, transport::failure("Server disconnected during play"); },
            StartConfiguration => { return play263::Clientbound.BundleDelimiter, transport::failure("Play reconfiguration is not implemented yet"); },
            _ => {},
        }
        if reply.len != 0 {
            let sent = transport::send(c, reply);
            if !transport::is_ok(sent) { return play263::Clientbound.BundleDelimiter, copy(sent); }
        }
    }
    return play263::Clientbound.BundleDelimiter, transport::failure("Terrain diagnostic packet limit reached");
}
