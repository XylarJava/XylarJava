// Development stream driver. Protocol bytes use stdin/stdout; results use
// stderr so tests can act as a peer without sockets or player directories.
import std;
import os;
import json;
import net.session;
fn result(c: transport::Connection&, error: transport::Error&, detail: const u8[:]) {
    var out: u8[>..] = [];
    format(out, "{\"ok\":", if transport::is_ok(error) { "true" } else { "false" }, ",\"detail\":");
    json::format_string(out, detail);
    out.append(",\"error\":"); json::format_string(out, error.message);
    format(out, ",\"sent\":", c.sent, ",\"received\":", c.received,
        ",\"bytes_in\":", c.bytes_in, ",\"bytes_out\":", c.bytes_out, "}\n");
    transport::close(c);
    write_stderr(out);
    if !transport::is_ok(error) { exit(1); }
}
fn main() {
    let a = args(); assert(a.len >= 2);
    var c = transport::new_detached(); c.fd = 0;
    transport::set_read_timeout(c, 1000);
    if a[1] == "login" || a[1] == "configure" || a[1] == "terrain" {
        let profile, error = session263::offline_login(c, "localhost", 25565, "Xylar");
        if (a[1] == "configure" || a[1] == "terrain") && transport::is_ok(error) {
            let configured, config_error = session263::configure(c);
            var detail: u8[>..] = [];
            format(detail, "registries:", configured.registries.len, " tags:", configured.tags.len, " features:", configured.features.len, " cookies:", configured.cookies.len);
            if !transport::is_ok(config_error) { result(c, config_error, detail); return; }
            if a[1] == "terrain" {
                var player = player_sync263::State {};
                let terrain, error = session263::receive_terrain(c,player,configured.dimensions);
                match terrain {
                    LevelChunkWithLight p => { format(detail, " chunk:", p.x, ",", p.z, " bytes:", p.chunk.buffer.len, " entities:", p.chunk.block_entities.len); },
                    _ => {},
                }
                if player.logged_in || player.positioned { format(detail," player:",player.player_id," teleports:",player.teleports); }
                if player.logged_in {
                    match terrain {
                        LevelChunkWithLight p => {
                            let column,e=column263::bind(p.x,p.z,p.chunk,p.light,player.layout,16,configured.biome_bits);
                            if !wire::is_ok(e) {let failure=transport::failure(wire::error_message(e));result(c,failure,detail);return;}
                            let first,found=column263::block_at(column,0,player.layout.min_y,0);
                            format(detail," min_y:",player.layout.min_y," sections:",column.sections.len," first:",first);
                        }, _=>{},
                    }
                }
                result(c, error, detail); return;
            }
            // Confirm that configuration did not consume the following play packet.
            let first_play, play_error = transport::recv(c);
            format(detail, " next:", first_play.id);
            result(c, play_error, detail);
        } else {
            let uuid = cryptography::uuid_string(profile.uuid);
            result(c, error, uuid);
        }
    } else if a[1] == "status" {
        let ping, error = session263::ping(c, "localhost", 25565);
        result(c, error, ping.json);
    } else {
        assert(a[1] == "echo" && a.len == 3);
        let count, ok = parse_int(a[2]); assert(ok && count >= 0);
        let key: u8[16] = [7; 16];
        let encryption = transport::enable_encryption(c, key); assert(transport::is_ok(encryption));
        transport::set_compression(c, 64);
        for i in count {
            let packet, error = transport::recv(c);
            if !transport::is_ok(error) { result(c, error, "echo read"); return; }
            var body: u8[>..] = [];
            wire::write_varint(body, packet.id); body.append(packet.payload);
            let sent = transport::send(c, body);
            if !transport::is_ok(sent) { result(c, sent, "echo write"); return; }
        }
        var error = transport::Error {};
        result(c, error, "echo complete");
    }
}
