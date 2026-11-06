// Headless tests of the actual Goose world/player code. No graphics or C shim.
import player;

fn test_mesh_boundaries() {
    world_clear();
    fn discard(x: i64, y: i64, z: i64, side: i64, material: u8) {}
    assert(world_mesh_chunk(0, discard) == 0);
    assert(world_set(15, 4, 8, STONE));
    assert(world_mesh_chunk(0, discard) == 6);
    assert(world_set(16, 4, 8, STONE));
    // A chunk boundary must not retain either hidden internal face.
    assert(world_mesh_chunk(0, discard) == 5);
    assert(world_mesh_chunk(1, discard) == 5);
    fill(world_dirty, false);
    assert(world_set(15, 4, 8, AIR));
    assert(world_dirty[0] && world_dirty[1]);
    for chunk in 2..CHUNK_COUNT { assert(!world_dirty[chunk]); }
    assert(world_mesh_chunk(1, discard) == 6);
    fill(world_dirty, false);
    assert(!world_set(16, 4, 8, STONE));
    for chunk in CHUNK_COUNT { assert(!world_dirty[chunk]); }
    assert(!world_set(-1, 0, 0, STONE));
    assert(!world_set(WORLD_X, 0, 0, STONE));
    assert(!world_set(0, WORLD_Y, 0, STONE));
    assert(!world_set(0, 0, WORLD_Z, STONE));
    assert(!world_set(0, 0, 0, 255));
    assert(world_get(-1, 0, 0) == AIR);
    print("mesh seams and dirty propagation passed");
}
fn world_checksum() -> u64 {
    var sum: u64 = 0;
    for i in world_blocks.len { sum += world_blocks[i] * ((i + 1) as u64); }
    return sum;
}
fn test_generation() {
    world_generate(42);
    let first = world_checksum();
    world_generate(42);
    assert(world_checksum() == first);
    world_generate(43);
    assert(world_checksum() != first);
    for seed in [0, 42, 1000000] {
        world_generate(seed);
        assert(!player_collides(player_spawn(seed)));
        for z in WORLD_Z { for x in WORLD_X { assert(world_get(x, 0, z) == BEDROCK); } }
    }
    print("deterministic generation and clear spawn passed");
}
fn flat_test_world() {
    world_clear();
    for z in WORLD_Z { for x in WORLD_X { world_set(x, 0, z, BEDROCK); } }
}
fn test_collision() {
    flat_test_world();
    var p = Player { x: 8.5, y: 10.0, z: 8.5 };
    for tick in 240 { player_tick(p, 0.0, 0.0, false, false); }
    assert(p.grounded && abs(p.y - 1.0) < 0.001 && !player_collides(p));
    player_tick(p, 0.0, 0.0, true, false);
    assert(p.y > 1.01 && p.vertical_speed > 0.0 && !p.grounded);
    for tick in 240 { player_tick(p, 0.0, 0.0, false, false); }
    assert(p.grounded && abs(p.y - 1.0) < 0.001);
    for y in 1..4 { world_set(10, y, 8, STONE); }
    for tick in 240 { player_tick(p, 0.0, 1.0, false, false); }
    assert(p.x >= 9.69 && p.x <= 9.70001 && !player_collides(p));
    p = Player { x: 8.5, y: 30.0, z: 8.5 };
    assert(!player_move_axis(p, -100.0, 1));
    assert(abs(p.y - 1.0) < 0.001 && !player_collides(p));
    // A low ceiling arrests the jump without placing the body in a solid cell.
    p = Player { x: 8.5, y: 1.001, z: 8.5, grounded: true };
    world_set(8, 3, 8, STONE);
    for tick in 12 { player_tick(p, 0.0, 0.0, true, false); }
    assert(p.y <= 1.20001 && !player_collides(p));
    print("landing, jump, wall, ceiling and fall tunneling passed");
}
fn test_movement_speed() {
    flat_test_world();
    var straight = Player { x: 32.5, y: 1.001, z: 32.5 };
    var diagonal = straight;
    for tick in 120 {
        player_tick(straight, 1.0, 0.0, false, false);
        player_tick(diagonal, 1.0, 1.0, false, false);
    }
    let a = 32.5 - straight.z;
    let dx = diagonal.x - 32.5;
    let dz = diagonal.z - 32.5;
    let b = sqrt(dx * dx + dz * dz);
    assert(abs(a - 4.317) < 0.0001 && abs(a - b) < 0.0001);
    print("diagonal speed normalization passed");
}
fn test_interaction() {
    world_clear();
    world_set(4, 2, 4, STONE);
    let hit = world_raycast(1.5, 2.5, 4.5, 1.0, 0.0, 0.0, 5.0);
    assert(hit.found && hit.x == 4 && hit.y == 2 && hit.z == 4 && hit.previous_x == 3);
    assert(!world_raycast(1.5, 2.5, 4.5, 1.0, 0.0, 0.0, 2.49).found);
    assert(world_raycast(1.5, 2.5, 4.5, 1.0, 0.0, 0.0, 2.5).found);
    assert(!world_raycast(1.5, 2.5, 4.5, 0.0, 0.0, 0.0, 5.0).found);
    let backwards = world_raycast(5.0, 2.5, 4.5, -1.0, 0.0, 0.0, 5.0);
    assert(backwards.found && backwards.x == 4 && backwards.previous_x == 5);
    let away = Player { x: 20.5, y: 2.0, z: 20.5 };
    assert(player_place(away, hit, WOOD));
    assert(world_get(3, 2, 4) == WOOD);
    assert(!player_place(away, hit, WOOD));
    assert(player_break(hit));
    assert(world_get(4, 2, 4) == AIR);
    world_set(4, 2, 4, BEDROCK);
    assert(!player_break(hit));
    world_set(3, 2, 4, AIR);
    let nearby = Player { x: 3.5, y: 2.0, z: 4.5 };
    assert(!player_place(nearby, hit, WOOD));
    world_set(0, 2, 4, STONE);
    let from_outside = world_raycast(-1.5, 2.5, 4.5, 1.0, 0.0, 0.0, 5.0);
    assert(from_outside.found && from_outside.x == 0);
    assert(!player_place(away, from_outside, WOOD));
    print("ray reach, axis rays, negative coordinates, break/place and player overlap passed");
}
fn main() {
    test_mesh_boundaries();
    test_generation();
    test_collision();
    test_movement_speed();
    test_interaction();
    print("Goose engine tests passed.");
}
