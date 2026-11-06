// Original finite voxel sandbox. These material IDs and terrain are NOT Mojang's.
import std;
import math;

let WORLD_X = 64;
let WORLD_Y = 32;
let WORLD_Z = 64;
let CHUNK_SIZE = 16;
let CHUNKS_X = WORLD_X / CHUNK_SIZE;
let CHUNKS_Z = WORLD_Z / CHUNK_SIZE;
let CHUNK_COUNT = CHUNKS_X * CHUNKS_Z;
let AIR: u8 = 0;
let GRASS: u8 = 1;
let DIRT: u8 = 2;
let STONE: u8 = 3;
let WOOD: u8 = 4;
let LEAVES: u8 = 5;
let BEDROCK: u8 = 6;
var world_blocks: u8[WORLD_X * WORLD_Y * WORLD_Z];
var world_dirty: bool[CHUNK_COUNT];

fn world_inside(x: i64, y: i64, z: i64) -> bool {
    x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y && z >= 0 && z < WORLD_Z
}
fn world_index(x: i64, y: i64, z: i64) -> i64 { (y * WORLD_Z + z) * WORLD_X + x }
fn world_get(x: i64, y: i64, z: i64) -> u8 {
    if !world_inside(x, y, z) { return AIR; }
    return world_blocks[world_index(x, y, z)];
}
fn world_mark_chunk(x: i64, z: i64) {
    if x >= 0 && x < WORLD_X && z >= 0 && z < WORLD_Z {
        world_dirty[(z / CHUNK_SIZE) * CHUNKS_X + x / CHUNK_SIZE] = true;
    }
}
fn world_set(x: i64, y: i64, z: i64, material: u8) -> bool {
    if !world_inside(x, y, z) || material > BEDROCK { return false; }
    let i = world_index(x, y, z);
    if world_blocks[i] == material { return false; }
    world_blocks[i] = material;
    world_mark_chunk(x, z);
    // Only face-sharing neighbors need rebuilding, including across chunk seams.
    if x % CHUNK_SIZE == 0 { world_mark_chunk(x - 1, z); }
    if x % CHUNK_SIZE == CHUNK_SIZE - 1 { world_mark_chunk(x + 1, z); }
    if z % CHUNK_SIZE == 0 { world_mark_chunk(x, z - 1); }
    if z % CHUNK_SIZE == CHUNK_SIZE - 1 { world_mark_chunk(x, z + 1); }
    return true;
}
fn world_clear() {
    fill(world_blocks, AIR);
    fill(world_dirty, true);
}
fn terrain_height(x: i64, z: i64, seed: i64) -> i64 {
    let phase = (seed % 997) as f64 * 0.01;
    return 9 + floor(sin(x * 0.14 + phase) * 2.5 + cos(z * 0.17 - phase) * 2.0) as i64;
}
fn world_generate(seed: i64) {
    world_clear();
    for z in WORLD_Z { for x in WORLD_X {
        let height = terrain_height(x, z, seed);
        for y in 0..height + 1 {
            let material = if y == 0 { BEDROCK } else if y == height { GRASS }
                        else if y >= height - 2 { DIRT } else { STONE };
            world_blocks[world_index(x, y, z)] = material;
        }
    } }
    // Sparse trees, with a clear spawn area and space for crowns at the borders.
    for z in 3..WORLD_Z - 3 { for x in 3..WORLD_X - 3 {
        if abs(x - WORLD_X / 2) < 5 && abs(z - WORLD_Z / 2) < 5 { continue; }
        if (x * 17 + z * 31 + seed % 997) % 113 != 0 { continue; }
        let base = terrain_height(x, z, seed) + 1;
        for y in base..base + 4 { world_set(x, y, z, WOOD); }
        for dz in -2..3 { for dx in -2..3 { for dy in 2..5 {
            if abs(dx) + abs(dz) > 3 { continue; }
            if world_get(x + dx, base + dy, z + dz) == AIR {
                world_set(x + dx, base + dy, z + dz, LEAVES);
            }
        } } }
    } }
}

// One exposed unit face, direction order: -X,+X,-Y,+Y,-Z,+Z.
// The callback is specialized at compile time; no per-material virtual dispatch.
fn world_mesh_chunk<Emit>(chunk: i64) -> i64 {
    assert(chunk >= 0 && chunk < CHUNK_COUNT);
    let start_x = (chunk % CHUNKS_X) * CHUNK_SIZE;
    let start_z = (chunk / CHUNKS_X) * CHUNK_SIZE;
    let dx: i64[6] = [-1, 1, 0, 0, 0, 0];
    let dy: i64[6] = [0, 0, -1, 1, 0, 0];
    let dz: i64[6] = [0, 0, 0, 0, -1, 1];
    var faces: i64 = 0;
    for y in WORLD_Y { for z in start_z..start_z + CHUNK_SIZE { for x in start_x..start_x + CHUNK_SIZE {
        let material = world_get(x, y, z);
        if material == AIR { continue; }
        for side in 6 {
            if world_get(x + dx[side], y + dy[side], z + dz[side]) == AIR {
                Emit(x, y, z, side, material);
                faces += 1;
            }
        }
    } } }
    return faces;
}
