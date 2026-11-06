// Native diagnostic entry point for translated protocol code. No game renderer.
import std;
import os;
import json;
import net.session;
fn fail(message: const u8[:]) {
    var output: u8[>..] = [];
    output.append("{\"ok\":false,\"error\":");
    json::format_string(output, message);
    output.push('}'); print(output);
    exit(1);
}
fn main() {
    let arguments = args();
    if arguments.len == 2 && arguments[1] == "--help" {
        print("Xylar Goose 26.3 network diagnostic; this executable does not render a game.");
        print("status HOST PORT [TIMEOUT_MS]");
        print("login-offline HOST PORT NAME [TIMEOUT_MS] (ends after login acknowledgement)");
        print("configure-offline HOST PORT NAME [TIMEOUT_MS] (collects registries and stops before gameplay)");
        print("terrain-offline HOST PORT NAME [TIMEOUT_MS] (decodes first chunk; no gameplay)");
        return;
    }
    if arguments.len < 4 { fail("Expected status HOST PORT or login-offline HOST PORT NAME"); }
    let mode = arguments[1];
    let terrain = mode == "terrain-offline";
    let configure = mode == "configure-offline" || terrain;
    let offline = mode == "login-offline" || configure;
    if mode != "status" && !offline { fail("Unknown command"); }
    let required: i64 = if offline { 5 } else { 4 };
    if arguments.len != required && arguments.len != required + 1 { fail("Invalid arguments"); }
    let port, port_ok = parse_int(arguments[3]);
    if !port_ok || port < 1 || port > 65535 { fail("Invalid port"); }
    var timeout: i64 = 5000;
    if arguments.len == required + 1 {
        let value, ok = parse_int(arguments[required]);
        if !ok || value < 1 || value > 60000 { fail("Invalid timeout"); }
        timeout = value;
    }
    let host = arguments[2];
    let units, host_error = wire::utf16_units(host);
    if !wire::is_ok(host_error) || units == 0 || units > 255 { fail("Invalid host"); }
    var c, connect_error = transport::connect(host, port as u16, timeout);
    if !transport::is_ok(connect_error) { fail(connect_error.message); }
    transport::set_read_timeout(c, timeout);
    if offline {
        let profile, error = session263::offline_login(c, host, port as u16, arguments[4]);
        if !transport::is_ok(error) { transport::close(c); fail(error.message); }
        var registry_count: i64 = 0;
        var dimensions:registry263::Layout[>..]=[]; var biome_bits:u8=0; var loaded_sections:i64=0;
        var chunk_x: i32 = 0; var chunk_z: i32 = 0; var section_bytes: i64 = 0;
        if configure {
            let configured, config_error = session263::configure(c);
            if !transport::is_ok(config_error) { transport::close(c); fail(config_error.message); }
            registry_count = configured.registries.len; dimensions.append(configured.dimensions);biome_bits=configured.biome_bits;
        }
        var player = player_sync263::State {};
        if terrain {
            let packet, error = session263::receive_terrain(c,player,dimensions);
            if !transport::is_ok(error) { transport::close(c); fail(error.message); }
            match packet {
                LevelChunkWithLight p => { chunk_x = p.x; chunk_z = p.z; section_bytes = p.chunk.buffer.len;
                    if player.logged_in {
                        // Protocol 777 diagnostic uses the pinned 16-bit block palette.
                        // Production resource-derived registry sizing remains pending.
                        let column,e=column263::bind(p.x,p.z,p.chunk,p.light,player.layout,16,biome_bits);
                        if !wire::is_ok(e) {transport::close(c);fail(wire::error_message(e));}
                        loaded_sections=column.sections.len;
                    }
                },
                _ => { transport::close(c); fail("Expected a terrain packet"); },
            }
        }
        transport::close(c);
        var output: u8[>..] = [];
        format(output, "{\"ok\":true,\"mode\":"); json::format_string(output, mode);
        output.append(",\"stage\":"); json::format_string(output, if configure { "play" } else { "configuration" });
        output.append(",\"playable\":false,\"name\":");
        json::format_string(output, profile.name);
        output.append(",\"uuid\":");
        let uuid = cryptography::uuid_string(profile.uuid);
        json::format_string(output, uuid);
        format(output, ",\"registries\":", registry_count, ",\"received\":", c.received, ",\"sent\":", c.sent);
        if terrain {
            format(output, ",\"chunk_x\":", chunk_x, ",\"chunk_z\":", chunk_z, ",\"section_bytes\":", section_bytes, ",\"logged_in\":", if player.logged_in { "true" } else { "false" }, ",\"positioned\":", if player.positioned { "true" } else { "false" }, ",\"teleports\":", player.teleports, ",\"sections\":", loaded_sections, ",\"min_y\":", player.layout.min_y);
        }
        output.push('}'); print(output);
    } else {
        let result, error = session263::ping(c, host, port as u16);
        transport::close(c);
        if !transport::is_ok(error) { fail(error.message); }
        var output: u8[>..] = [];
        format(output, "{\"ok\":true,\"mode\":\"status\",\"target\":\"26.3\",\"latency_ms\":", result.latency_ms, ",\"status\":", result.json,
               ",\"received\":", c.received, ",\"sent\":", c.sent, "}");
        print(output);
    }
}
