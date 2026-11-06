// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Terrain/control subset of pinned packets/play.rs, protocol 777.
// Other play packets remain explicit Unknown values. See port-ledger.json.
import std;
import net.chunk;
import core.position;
namespace play263;

// Floating fields retain exact IEEE bits at the wire boundary, including NaNs.
// The session converts them through the existing native bitcast adapter.
struct OptionalInt { present: bool = false, value: i32 = 0 }
struct OptionalGlobalPos { present: bool = false, dimension: u8[] = [], packed: i64 = 0 }
struct LevelName { value: u8[] = [] }
fn read_previous(r: wire::Reader&) -> OptionalInt, wire::Error {
    let v, e = wire::read_varint(r);
    return OptionalInt { present: v != 0, value: ((v as i64) - 1) as! i32 }, e;
}
fn write_previous(v: OptionalInt&, out: u8[>..]&) { wire::write_varint(out, if v.present { ((v.value as i64) + 1) as! i32 } else { 0 }); }
fn read_death(r: wire::Reader&) -> OptionalGlobalPos, wire::Error {
    let present, e = wire::read_bool(r); if !wire::is_ok(e) || !present { return OptionalGlobalPos {}, e; }
    let dimension, a = wire::read_string(r,32767); if !wire::is_ok(a) { return OptionalGlobalPos {}, a; }
    let packed, b = wire::read_i64(r); return OptionalGlobalPos { present: true, dimension: dimension, packed: packed }, b;
}
fn write_death(v: OptionalGlobalPos&, out: u8[>..]&) {
    wire::write_bool(out,v.present); if v.present { wire::write_string(out,v.dimension); wire::write_i64(out,v.packed); }
}
fn read_levels(r: wire::Reader&) -> LevelName[>..], wire::Error {
    let n,e = wire::read_count(r,wire::remaining(r)); if !wire::is_ok(e) { return [],e; }
    var levels: LevelName[>..] = [];
    for i in n { let v,a = wire::read_string(r,32767); if !wire::is_ok(a) { return [],a; } levels.push(LevelName { value: v }); }
    return levels, wire::Error.Ok;
}
fn write_levels(v: LevelName[:], out: u8[>..]&) { wire::write_varint(out,v.len as! i32); for l in v { wire::write_string(out,l.value); } }
fn read_vector(r: wire::Reader&) -> i64[3], wire::Error {
    var v: i64[3] = [0;3]; for i in 3 { let x,e = wire::read_i64(r); if !wire::is_ok(e) { return [0;3],e; } v[i]=x; } return v,wire::Error.Ok;
}
fn write_vector(v: i64[:], out: u8[>..]&) { for x in v { wire::write_i64(out,x); } }
struct SpawnInfo { dimension_type: i32 = 0, dimension: u8[] = [], seed: i64 = 0, game_type: i32 = 0, previous_game_type: OptionalInt = OptionalInt {}, debug: bool = false, flat: bool = false, last_death: OptionalGlobalPos = OptionalGlobalPos {}, portal_cooldown: i32 = 0, sea_level: i32 = 0 }
fn read_SpawnInfo(r: wire::Reader&) -> SpawnInfo, wire::Error {
    let dimension_type, e_dimension_type = wire::read_varint(r); if !wire::is_ok(e_dimension_type) { return SpawnInfo {}, e_dimension_type; }
    let dimension, e_dimension = wire::read_string(r, 32767); if !wire::is_ok(e_dimension) { return SpawnInfo {}, e_dimension; }
    let seed, e_seed = wire::read_i64(r); if !wire::is_ok(e_seed) { return SpawnInfo {}, e_seed; }
    let game_type, e_game_type = wire::read_varint(r); if !wire::is_ok(e_game_type) { return SpawnInfo {}, e_game_type; }
    let previous_game_type, e_previous_game_type = read_previous(r); if !wire::is_ok(e_previous_game_type) { return SpawnInfo {}, e_previous_game_type; }
    let debug, e_debug = wire::read_bool(r); if !wire::is_ok(e_debug) { return SpawnInfo {}, e_debug; }
    let flat, e_flat = wire::read_bool(r); if !wire::is_ok(e_flat) { return SpawnInfo {}, e_flat; }
    let last_death, e_last_death = read_death(r); if !wire::is_ok(e_last_death) { return SpawnInfo {}, e_last_death; }
    let portal_cooldown, e_portal_cooldown = wire::read_varint(r); if !wire::is_ok(e_portal_cooldown) { return SpawnInfo {}, e_portal_cooldown; }
    let sea_level, e_sea_level = wire::read_varint(r); if !wire::is_ok(e_sea_level) { return SpawnInfo {}, e_sea_level; }
    return SpawnInfo { dimension_type: dimension_type, dimension: dimension, seed: seed, game_type: game_type, previous_game_type: previous_game_type, debug: debug, flat: flat, last_death: copy(last_death), portal_cooldown: portal_cooldown, sea_level: sea_level }, wire::Error.Ok;
}
fn write_SpawnInfo(v: SpawnInfo&, out: u8[>..]&) {
    wire::write_varint(out, v.dimension_type);
    wire::write_string(out, v.dimension);
    wire::write_i64(out, v.seed);
    wire::write_varint(out, v.game_type);
    write_previous(v.previous_game_type, out);
    wire::write_bool(out, v.debug);
    wire::write_bool(out, v.flat);
    write_death(v.last_death, out);
    wire::write_varint(out, v.portal_cooldown);
    wire::write_varint(out, v.sea_level);
}
struct Login { player_id: i32 = 0, hardcore: bool = false, levels: LevelName[] = [], max_players: i32 = 0, chunk_radius: i32 = 0, simulation_distance: i32 = 0, reduced_debug_info: bool = false, show_death_screen: bool = false, limited_crafting: bool = false, spawn: SpawnInfo = SpawnInfo {}, online_mode: bool = false, enforces_secure_chat: bool = false }
fn read_Login(r: wire::Reader&) -> Login, wire::Error {
    let player_id, e_player_id = wire::read_i32(r); if !wire::is_ok(e_player_id) { return Login {}, e_player_id; }
    let hardcore, e_hardcore = wire::read_bool(r); if !wire::is_ok(e_hardcore) { return Login {}, e_hardcore; }
    let levels, e_levels = read_levels(r); if !wire::is_ok(e_levels) { return Login {}, e_levels; }
    let max_players, e_max_players = wire::read_varint(r); if !wire::is_ok(e_max_players) { return Login {}, e_max_players; }
    let chunk_radius, e_chunk_radius = wire::read_varint(r); if !wire::is_ok(e_chunk_radius) { return Login {}, e_chunk_radius; }
    let simulation_distance, e_simulation_distance = wire::read_varint(r); if !wire::is_ok(e_simulation_distance) { return Login {}, e_simulation_distance; }
    let reduced_debug_info, e_reduced_debug_info = wire::read_bool(r); if !wire::is_ok(e_reduced_debug_info) { return Login {}, e_reduced_debug_info; }
    let show_death_screen, e_show_death_screen = wire::read_bool(r); if !wire::is_ok(e_show_death_screen) { return Login {}, e_show_death_screen; }
    let limited_crafting, e_limited_crafting = wire::read_bool(r); if !wire::is_ok(e_limited_crafting) { return Login {}, e_limited_crafting; }
    let spawn, e_spawn = read_SpawnInfo(r); if !wire::is_ok(e_spawn) { return Login {}, e_spawn; }
    let online_mode, e_online_mode = wire::read_bool(r); if !wire::is_ok(e_online_mode) { return Login {}, e_online_mode; }
    let enforces_secure_chat, e_enforces_secure_chat = wire::read_bool(r); if !wire::is_ok(e_enforces_secure_chat) { return Login {}, e_enforces_secure_chat; }
    return Login { player_id: player_id, hardcore: hardcore, levels: levels[..], max_players: max_players, chunk_radius: chunk_radius, simulation_distance: simulation_distance, reduced_debug_info: reduced_debug_info, show_death_screen: show_death_screen, limited_crafting: limited_crafting, spawn: copy(spawn), online_mode: online_mode, enforces_secure_chat: enforces_secure_chat }, wire::Error.Ok;
}
fn write_Login(v: Login&, out: u8[>..]&) {
    wire::write_i32(out, v.player_id);
    wire::write_bool(out, v.hardcore);
    write_levels(v.levels, out);
    wire::write_varint(out, v.max_players);
    wire::write_varint(out, v.chunk_radius);
    wire::write_varint(out, v.simulation_distance);
    wire::write_bool(out, v.reduced_debug_info);
    wire::write_bool(out, v.show_death_screen);
    wire::write_bool(out, v.limited_crafting);
    write_SpawnInfo(v.spawn, out);
    wire::write_bool(out, v.online_mode);
    wire::write_bool(out, v.enforces_secure_chat);
}
struct Respawn { spawn: SpawnInfo = SpawnInfo {}, keep: i8 = 0 }
fn read_Respawn(r: wire::Reader&) -> Respawn, wire::Error {
    let spawn, e_spawn = read_SpawnInfo(r); if !wire::is_ok(e_spawn) { return Respawn {}, e_spawn; }
    let keep, e_keep = wire::read_i8(r); if !wire::is_ok(e_keep) { return Respawn {}, e_keep; }
    return Respawn { spawn: copy(spawn), keep: keep }, wire::Error.Ok;
}
fn write_Respawn(v: Respawn&, out: u8[>..]&) {
    write_SpawnInfo(v.spawn, out);
    wire::write_i8(out, v.keep);
}
struct PlayerPosition { teleport: i32 = 0, position_bits: i64[3] = [0; 3], delta_bits: i64[3] = [0; 3], yaw_bits: u32 = 0, pitch_bits: u32 = 0, relatives: i32 = 0 }
fn read_PlayerPosition(r: wire::Reader&) -> PlayerPosition, wire::Error {
    let teleport, e_teleport = wire::read_varint(r); if !wire::is_ok(e_teleport) { return PlayerPosition {}, e_teleport; }
    let position_bits, e_position_bits = read_vector(r); if !wire::is_ok(e_position_bits) { return PlayerPosition {}, e_position_bits; }
    let delta_bits, e_delta_bits = read_vector(r); if !wire::is_ok(e_delta_bits) { return PlayerPosition {}, e_delta_bits; }
    let yaw_bits, e_yaw_bits = wire::read_u32(r); if !wire::is_ok(e_yaw_bits) { return PlayerPosition {}, e_yaw_bits; }
    let pitch_bits, e_pitch_bits = wire::read_u32(r); if !wire::is_ok(e_pitch_bits) { return PlayerPosition {}, e_pitch_bits; }
    let relatives, e_relatives = wire::read_i32(r); if !wire::is_ok(e_relatives) { return PlayerPosition {}, e_relatives; }
    return PlayerPosition { teleport: teleport, position_bits: position_bits, delta_bits: delta_bits, yaw_bits: yaw_bits, pitch_bits: pitch_bits, relatives: relatives }, wire::Error.Ok;
}
fn write_PlayerPosition(v: PlayerPosition&, out: u8[>..]&) {
    wire::write_varint(out, v.teleport);
    write_vector(v.position_bits, out);
    write_vector(v.delta_bits, out);
    wire::write_u32(out, v.yaw_bits);
    wire::write_u32(out, v.pitch_bits);
    wire::write_i32(out, v.relatives);
}

enum Clientbound {
    BlockUpdate { packed_pos: i64, state: i32 },
    ForgetLevelChunk { packed_pos: i64 },
    SectionBlocksUpdate { packed_section: i64, changes: i64[] },
    Login { value: Login },
    Respawn { value: Respawn },
    PlayerPosition { value: PlayerPosition },
    BundleDelimiter,
    ChunkBatchStart,
    ChunkBatchFinished { size: i32 },
    KeepAlive { id: i64 },
    Ping { id: i32 },
    Disconnect { reason: u8[] },
    StartConfiguration,
    LevelChunkWithLight { x: i32, z: i32, chunk: chunk_wire::ChunkData, light: chunk_wire::LightData },
    LightUpdate { x: i32, z: i32, light: chunk_wire::LightData },
    Unknown { id: i32, payload: u8[] },
}
fn read_clientbound(r: wire::Reader&, id: i32) -> Clientbound.., wire::Error {
    if id == 50 { let v,e = read_Login(r); return Clientbound.Login { value: copy(v) }, e; }
    if id == 84 { let v,e = read_Respawn(r); return Clientbound.Respawn { value: copy(v) }, e; }
    if id == 73 { let v,e = read_PlayerPosition(r); return Clientbound.PlayerPosition { value: copy(v) }, e; }
    if id == 8 {
        let pos,e=wire::read_i64(r); if !wire::is_ok(e) {return Clientbound.BundleDelimiter,e;}
        let state,f=wire::read_varint(r);return Clientbound.BlockUpdate {packed_pos:pos,state:state},f;
    }
    if id == 38 {let pos,e=wire::read_i64(r);return Clientbound.ForgetLevelChunk {packed_pos:pos},e;}
    if id == 86 {
        let pos,e=wire::read_i64(r);if !wire::is_ok(e) {return Clientbound.BundleDelimiter,e;}
        let n,f=wire::read_count(r,wire::remaining(r));if !wire::is_ok(f) {return Clientbound.BundleDelimiter,f;}
        var changes:i64[>..]=[];
        for i in n {let v,g=wire::read_varlong(r);if !wire::is_ok(g) {return Clientbound.BundleDelimiter,g;}changes.push(v);}
        return Clientbound.SectionBlocksUpdate {packed_section:pos,changes:changes[..]},wire::Error.Ok;
    }
    if id == 0 { return Clientbound.BundleDelimiter, wire::Error.Ok; }
    if id == 12 { return Clientbound.ChunkBatchStart, wire::Error.Ok; }
    if id == 120 { return Clientbound.StartConfiguration, wire::Error.Ok; }
    if id == 11 {
        let size, e = wire::read_varint(r); return Clientbound.ChunkBatchFinished { size: size }, e;
    }
    if id == 45 { let value, e = wire::read_i64(r); return Clientbound.KeepAlive { id: value }, e; }
    if id == 62 { let value, e = wire::read_i32(r); return Clientbound.Ping { id: value }, e; }
    if id == 32 { let reason, e = wire::read_nbt(r); return Clientbound.Disconnect { reason: reason[..] }, e; }
    if id == 46 || id == 49 {
        var x: i32 = 0; var z: i32 = 0;
        if id == 46 {
            let a, e = wire::read_i32(r); if !wire::is_ok(e) { return Clientbound.BundleDelimiter, e; } x = a;
            let b, f = wire::read_i32(r); if !wire::is_ok(f) { return Clientbound.BundleDelimiter, f; } z = b;
            let chunk, ce = chunk_wire::read_chunk(r); if !wire::is_ok(ce) { return Clientbound.BundleDelimiter, ce; }
            let light, le = chunk_wire::read_light(r); if !wire::is_ok(le) { return Clientbound.BundleDelimiter, le; }
            return Clientbound.LevelChunkWithLight { x: x, z: z, chunk: copy(chunk), light: copy(light) }, wire::Error.Ok;
        }
        let a, e = wire::read_varint(r); if !wire::is_ok(e) { return Clientbound.BundleDelimiter, e; } x = a;
        let b, f = wire::read_varint(r); if !wire::is_ok(f) { return Clientbound.BundleDelimiter, f; } z = b;
        let light, le = chunk_wire::read_light(r); if !wire::is_ok(le) { return Clientbound.BundleDelimiter, le; }
        return Clientbound.LightUpdate { x: x, z: z, light: copy(light) }, wire::Error.Ok;
    }
    return Clientbound.Unknown { id: id, payload: wire::rest(r) }, wire::Error.Ok;
}
fn decode_clientbound(id: i32, bytes: const u8[:]) -> Clientbound.., wire::Error {
    var r = wire::reader(bytes); let p, e = read_clientbound(r, id);
    if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; }
    if wire::remaining(r) != 0 { return Clientbound.Unknown { id: id, payload: [] }, wire::Error.PacketTooLarge { id: id, serverbound: false, extra: wire::remaining(r) }; }
    return copy(p), wire::Error.Ok;
}
fn encode_clientbound(p: Clientbound..&) -> u8[>..] {
    var out: u8[>..] = [];
    match p {
        BlockUpdate p => {wire::write_varint(out,8);wire::write_i64(out,p.packed_pos);wire::write_varint(out,p.state);},
        ForgetLevelChunk p => {wire::write_varint(out,38);wire::write_i64(out,p.packed_pos);},
        SectionBlocksUpdate p => {wire::write_varint(out,86);wire::write_i64(out,p.packed_section);wire::write_varint(out,p.changes.len as! i32);for v in p.changes {wire::write_varlong(out,v);}},
        PlayerPosition p => { wire::write_varint(out,73); write_PlayerPosition(p.value,out); },
        Respawn p => { wire::write_varint(out,84); write_Respawn(p.value,out); },
        Login p => { wire::write_varint(out,50); write_Login(p.value,out); },
        BundleDelimiter => { wire::write_varint(out, 0); },
        ChunkBatchStart => { wire::write_varint(out, 12); },
        ChunkBatchFinished p => { wire::write_varint(out, 11); wire::write_varint(out, p.size); },
        KeepAlive p => { wire::write_varint(out, 45); wire::write_i64(out, p.id); },
        Ping p => { wire::write_varint(out, 62); wire::write_i32(out, p.id); },
        Disconnect p => { wire::write_varint(out, 32); wire::write_nbt(out, p.reason); },
        StartConfiguration => { wire::write_varint(out, 120); },
        LevelChunkWithLight p => { wire::write_varint(out, 46); wire::write_i32(out, p.x); wire::write_i32(out, p.z); chunk_wire::write_chunk(p.chunk, out); chunk_wire::write_light(p.light, out); },
        LightUpdate p => { wire::write_varint(out, 49); wire::write_varint(out, p.x); wire::write_varint(out, p.z); chunk_wire::write_light(p.light, out); },
        Unknown p => { wire::write_varint(out, p.id); out.append(p.payload); },
    }
    return out;
}

// remote.rs SectionBlocksUpdate coordinate/state extraction. Wire local order
// is X/Z/Y, whereas resident section indexing is Y/Z/X.
fn section_change(packed_section: i64, change: i64) -> position::BlockPos, i32 {
    let section=position::section_unpack(packed_section);
    return position::relative_to_block(section,(change & 4095) as u16),(change >> 12) as! i32;
}
