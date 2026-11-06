import std;
import os;
import player;

extern fn gx_open(hidden: i32) -> i32;
extern fn gx_close();
extern fn gx_poll() -> i32;
extern fn gx_active() -> i32;
extern fn gx_keys() -> i32;
extern fn gx_mouse_x() -> i32;
extern fn gx_mouse_y() -> i32;
extern fn gx_actions() -> i32;
extern fn gx_selected() -> i32;
extern fn gx_clock() -> f64;
extern fn gx_idle();
extern fn gx_mesh_begin(chunk: i64) -> i32;
extern fn gx_face(x: i64, y: i64, z: i64, side: i64, material: i32) -> i32;
extern fn gx_mesh_end() -> i32;
extern fn gx_target(visible: i32, x: i64, y: i64, z: i64);
extern fn gx_present(x: f64, y: f64, z: f64, yaw: f64, pitch: f64, selected: i32) -> i32;
extern fn gx_screenshot() -> i32;

fn upload_dirty_chunks() {
    for chunk in CHUNK_COUNT {
        if !world_dirty[chunk] { continue; }
        assert(gx_mesh_begin(chunk) != 0);
        world_mesh_chunk(chunk) { x, y, z, side, material =>
            assert(gx_face(x, y, z, side, material) != 0);
        };
        assert(gx_mesh_end() != 0);
        world_dirty[chunk] = false;
    }
}
fn main() {
    let arguments = args();
    var seed: i64 = 42;
    var frames: i64 = 0;
    var i: i64 = 1;
    while i < arguments.len {
        let option = arguments[i];
        if option == "--help" {
            print("Xylar Goose sandbox prototype (not yet Minecraft compatible)");
            print("--seed N (0..1000000), --frames N (1..10000, hidden repeatable preview)");
            print("WASD: walk, mouse: look, Space: jump, Shift: sprint, Esc: pause/resume");
            print("Left click: break, right click: place, 1-5/wheel: select material. No saves yet.");
            return;
        }
        if (option != "--seed" && option != "--frames") || i + 1 >= arguments.len {
            print("Unknown or incomplete option: ", option); exit(2);
        }
        let value, ok = parse_int(arguments[i + 1]);
        if !ok || value < 0 || value > 1000000 { print("Invalid option value."); exit(2); }
        if option == "--seed" { seed = value; }
        else {
            if value < 1 || value > 10000 { print("--frames must be 1..10000"); exit(2); }
            frames = value;
        }
        i += 2;
    }
    world_generate(seed);
    var player = player_spawn(seed);
    assert(!player_collides(player));
    let preview: i32 = if frames > 0 { 1 } else { 0 };
    if gx_open(preview) == 0 { exit(1); }
    upload_dirty_chunks();
    var previous = gx_clock();
    var accumulator: f64 = 0.0;
    var rendered: i64 = 0;
    while gx_poll() != 0 {
        let now = gx_clock();
        let elapsed = clamp(now - previous, 0.0, 0.1);
        previous = now;
        if gx_active() != 0 || frames > 0 {
            if frames == 0 {
                player.yaw += gx_mouse_x() * 0.0025;
                player.pitch = clamp(player.pitch - gx_mouse_y() * 0.0025, -1.55, 1.55);
            }
            let keys = if frames > 0 { 0 } else { gx_keys() };
            let forward: f64 = (if (keys & 1) != 0 { 1.0 } else { 0.0 }) -
                               (if (keys & 2) != 0 { 1.0 } else { 0.0 });
            let strafe: f64 = (if (keys & 8) != 0 { 1.0 } else { 0.0 }) -
                              (if (keys & 4) != 0 { 1.0 } else { 0.0 });
            accumulator += if frames > 0 { FIXED_DT } else { elapsed };
            while accumulator >= FIXED_DT {
                player_tick(player, forward, strafe, (keys & 16) != 0, (keys & 32) != 0);
                accumulator -= FIXED_DT;
            }
            var hit = player_target(player);
            let actions = if frames > 0 { 0 } else { gx_actions() };
            if (actions & 1) != 0 { player_break(hit); }
            if (actions & 2) != 0 { player_place(player, player_target(player), gx_selected() as u8); }
            hit = player_target(player);
            gx_target(if hit.found { 1 } else { 0 }, hit.x, hit.y, hit.z);
        } else {
            accumulator = 0.0;
            gx_idle();
        }
        upload_dirty_chunks();
        if gx_present(player.x, player.y + EYE_HEIGHT, player.z, player.yaw, player.pitch, gx_selected()) == 0 {
            gx_close();
            exit(1);
        }
        rendered += 1;
        if frames > 0 && rendered >= frames {
            if gx_screenshot() == 0 { print("Screenshot failed."); gx_close(); exit(1); }
            print("Rendered ", rendered, " sandbox frames to goose-preview.ppm");
            break;
        }
    }
    gx_close();
}
