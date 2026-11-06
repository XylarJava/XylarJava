// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Pose/acknowledgement subset of game/remote.rs::remote_teleport.
// Also ports player_sync.rs movement selection and teleport velocity.
// Physics, camera, input/tick loop and loading tracker remain pending.
import std;
import core.jmath;
import core.trig;
import net.play;
import net.registry;
namespace player_sync263;
struct State {
    layout: registry263::Layout = registry263::Layout {},
    logged_in: bool = false, positioned: bool = false,
    player_id: i32 = 0, dimension_type: i32 = 0, game_type: i32 = 0,
    x: f64 = 0.0, y: f64 = 0.0, z: f64 = 0.0, yaw: f32 = 0.0, pitch: f32 = 0.0,
    teleports: i64 = 0, velocity: trig263::Vec3 = trig263::Vec3 {},
}
fn login(state: State&, p: play263::Login&) {
    state.logged_in = true; state.player_id = p.player_id;
    state.dimension_type = p.spawn.dimension_type; state.game_type = p.spawn.game_type;
}
fn coordinate(flags: i32, bit: i32, base: f64, bits: i64) -> f64 {
    let value = jmath::gx_f64_from_bits(bits as! u64);
    return if (flags & bit) != 0 { base + value } else { value };
}
// PositionMoveRotation.calculateAbsolute: preserve f32 addition and NaN
// behavior of Rust clamp. Do not normalize yaw or change the addition order.
fn teleport(state: State&, p: play263::PlayerPosition&) -> u8[>..] {
    state.x = coordinate(p.relatives,1,state.x,p.position_bits[0]);
    state.y = coordinate(p.relatives,2,state.y,p.position_bits[1]);
    state.z = coordinate(p.relatives,4,state.z,p.position_bits[2]);
    let old_yaw = state.yaw; let old_pitch = state.pitch;
    let yaw = jmath::gx_f32_from_bits(p.yaw_bits);
    state.yaw = if (p.relatives & 8) != 0 { state.yaw + yaw } else { yaw };
    let offset: f32 = if (p.relatives & 16) != 0 { state.pitch } else { 0.0 };
    var pitch: f32 = offset + jmath::gx_f32_from_bits(p.pitch_bits);
    if pitch < -90.0 { pitch = -90.0; }
    if pitch > 90.0 { pitch = 90.0; }
    state.velocity = teleport_velocity(state.velocity,old_yaw,old_pitch,state.yaw,pitch,p);
    state.pitch = pitch; state.positioned = true; state.teleports += 1;
    var out: u8[>..] = [];
    wire::write_varint(out,0); wire::write_varint(out,p.teleport);
    wire::write_i64(out,jmath::gx_f64_bits(state.x) as! i64);
    wire::write_i64(out,jmath::gx_f64_bits(state.y) as! i64);
    wire::write_i64(out,jmath::gx_f64_bits(state.z) as! i64);
    wire::write_u32(out,jmath::gx_f32_bits(state.yaw));
    wire::write_u32(out,jmath::gx_f32_bits(state.pitch));
    return out;
}

fn teleport_velocity(v: trig263::Vec3, old_yaw: f32, old_pitch: f32, yaw: f32, pitch: f32, p: play263::PlayerPosition&) -> trig263::Vec3 {
    var velocity = v;
    if (p.relatives & 256) != 0 {
        let x_angle = (((old_pitch - pitch) as f64) * 0.017453292519943295) as f32;
        let y_angle = (((old_yaw - yaw) as f64) * 0.017453292519943295) as f32;
        velocity = trig263::rotate_y(trig263::rotate_x(velocity,x_angle),y_angle);
    }
    return trig263::Vec3 { x: coordinate(p.relatives,32,velocity.x,p.delta_bits[0]),
        y: coordinate(p.relatives,64,velocity.y,p.delta_bits[1]), z: coordinate(p.relatives,128,velocity.z,p.delta_bits[2]) };
}
// LocalPlayer.sendPosition baseline: teleport acknowledgements never mutate it.
struct PositionSender {
    x: f64 = 0.0, y: f64 = 0.0, z: f64 = 0.0, yaw: f32 = 0.0, pitch: f32 = 0.0,
    on_ground: bool = false, collision: bool = false, reminder: i64 = 0,
}
fn movement(sender: PositionSender&, state: State&, on_ground: bool, collision: bool) -> u8[>..] {
    sender.reminder += 1;
    let dx = state.x - sender.x; let dy = state.y - sender.y; let dz = state.z - sender.z;
    let moved = dx*dx + dy*dy + dz*dz > 2.0e-4 * 2.0e-4 || sender.reminder >= 20;
    let rotated = state.yaw - sender.yaw != 0.0 || state.pitch - sender.pitch != 0.0;
    let flags: u8 = ((if on_ground { 1 } else { 0 }) | (if collision { 2 } else { 0 })) as u8;
    var out: u8[>..] = [];
    if moved || rotated || sender.on_ground != on_ground || sender.collision != collision {
        wire::write_varint(out, if moved { if rotated { 31 } else { 30 } } else { if rotated { 32 } else { 33 } });
        if moved {
            wire::write_i64(out,jmath::gx_f64_bits(state.x) as! i64);
            wire::write_i64(out,jmath::gx_f64_bits(state.y) as! i64);
            wire::write_i64(out,jmath::gx_f64_bits(state.z) as! i64);
        }
        if rotated { wire::write_u32(out,jmath::gx_f32_bits(state.yaw)); wire::write_u32(out,jmath::gx_f32_bits(state.pitch)); }
        wire::write_u8(out,flags);
    }
    if moved { sender.x=state.x; sender.y=state.y; sender.z=state.z; sender.reminder=0; }
    if rotated { sender.yaw=state.yaw; sender.pitch=state.pitch; }
    sender.on_ground=on_ground; sender.collision=collision;
    return out;
}
