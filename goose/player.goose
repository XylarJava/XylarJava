import world;

let PLAYER_RADIUS: f64 = 0.3;
let PLAYER_HEIGHT: f64 = 1.8;
let EYE_HEIGHT: f64 = 1.62;
let FIXED_DT: f64 = 1.0 / 120.0;
struct Player {
    x: f64 = 0.0, y: f64 = 0.0, z: f64 = 0.0,
    yaw: f64 = 0.0, pitch: f64 = 0.0,
    vertical_speed: f64 = 0.0,
    grounded: bool = false,
}
struct RayHit {
    found: bool = false,
    x: i64 = 0, y: i64 = 0, z: i64 = 0,
    previous_x: i64 = 0, previous_y: i64 = 0, previous_z: i64 = 0,
}
fn player_spawn(seed: i64) -> Player {
    Player { x: WORLD_X / 2.0 + 0.5, y: terrain_height(WORLD_X / 2, WORLD_Z / 2, seed) + 1.001,
             z: WORLD_Z / 2.0 + 0.5, yaw: 0.0, pitch: -0.15 }
}
fn player_overlaps(p: Player, x: i64, y: i64, z: i64) -> bool {
    p.x + PLAYER_RADIUS > x && p.x - PLAYER_RADIUS < x + 1 &&
    p.y + PLAYER_HEIGHT > y && p.y < y + 1 &&
    p.z + PLAYER_RADIUS > z && p.z - PLAYER_RADIUS < z + 1
}
fn player_collides(p: Player) -> bool {
    if p.x - PLAYER_RADIUS < 0.0 || p.x + PLAYER_RADIUS > WORLD_X ||
       p.z - PLAYER_RADIUS < 0.0 || p.z + PLAYER_RADIUS > WORLD_Z || p.y < 0.0 { return true; }
    let epsilon: f64 = 0.000001;
    for y in (floor(p.y + epsilon) as i64)..(floor(p.y + PLAYER_HEIGHT - epsilon) as i64) + 1 {
        for z in (floor(p.z - PLAYER_RADIUS + epsilon) as i64)..(floor(p.z + PLAYER_RADIUS - epsilon) as i64) + 1 {
            for x in (floor(p.x - PLAYER_RADIUS + epsilon) as i64)..(floor(p.x + PLAYER_RADIUS - epsilon) as i64) + 1 {
                if world_get(x, y, z) != AIR { return true; }
            }
        }
    }
    return false;
}
// Axis-separated swept substeps. The maximum displacement is kept below a
// fraction of a block, so high fall speed cannot tunnel through a one-block floor.
fn player_move_axis(p: Player&, amount: f64, axis: i64) -> bool {
    let steps = max(1, ceil(abs(amount) / 0.1) as i64);
    let step = amount / steps;
    for i in steps {
        var candidate = p;
        if axis == 0 { candidate.x += step; }
        else if axis == 1 { candidate.y += step; }
        else { candidate.z += step; }
        if player_collides(candidate) {
            // Resolve to contact rather than hovering a whole substep above it.
            var lo: f64 = 0.0;
            var hi: f64 = 1.0;
            for refine in 16 {
                let mid = (lo + hi) * 0.5;
                candidate = p;
                if axis == 0 { candidate.x += step * mid; }
                else if axis == 1 { candidate.y += step * mid; }
                else { candidate.z += step * mid; }
                if player_collides(candidate) { hi = mid; } else { lo = mid; }
            }
            if axis == 0 { p.x += step * lo; }
            else if axis == 1 { p.y += step * lo; }
            else { p.z += step * lo; }
            return false;
        }
        p = candidate;
    }
    return true;
}
fn player_tick(p: Player&, forward: f64, strafe: f64, jump: bool, sprint: bool) {
    let magnitude = sqrt(forward * forward + strafe * strafe);
    let divisor = max(1.0, magnitude);
    let speed = if sprint { 5.6 } else { 4.317 };
    let dx = (sin(p.yaw) * forward + cos(p.yaw) * strafe) / divisor * speed * FIXED_DT;
    let dz = (-cos(p.yaw) * forward + sin(p.yaw) * strafe) / divisor * speed * FIXED_DT;
    player_move_axis(p, dx, 0);
    player_move_axis(p, dz, 2);
    if jump && p.grounded { p.vertical_speed = 8.0; }
    p.vertical_speed = max(-50.0, p.vertical_speed - 24.0 * FIXED_DT);
    let falling = p.vertical_speed < 0.0;
    let moved = player_move_axis(p, p.vertical_speed * FIXED_DT, 1);
    p.grounded = !moved && falling;
    if !moved { p.vertical_speed = 0.0; }
}

// Amanatides/Woo traversal. Zero direction components never divide by zero.
// The reach is measured along a normalized direction, in world blocks.
fn world_raycast(ox: f64, oy: f64, oz: f64, dx: f64, dy: f64, dz: f64, reach: f64) -> RayHit {
    var hit = RayHit { x: floor(ox) as i64, y: floor(oy) as i64, z: floor(oz) as i64 };
    hit.previous_x = hit.x;
    hit.previous_y = hit.y;
    hit.previous_z = hit.z;
    let length = sqrt(dx * dx + dy * dy + dz * dz);
    if length < 0.0000001 || reach < 0.0 { return hit; }
    let vx = dx / length;
    let vy = dy / length;
    let vz = dz / length;
    let sx: i64 = if vx < 0.0 { -1 } else { 1 };
    let sy: i64 = if vy < 0.0 { -1 } else { 1 };
    let sz: i64 = if vz < 0.0 { -1 } else { 1 };
    let tx = if abs(vx) < 0.0000001 { 1.0e30 } else { abs(1.0 / vx) };
    let ty = if abs(vy) < 0.0000001 { 1.0e30 } else { abs(1.0 / vy) };
    let tz = if abs(vz) < 0.0000001 { 1.0e30 } else { abs(1.0 / vz) };
    var mx = if vx < 0.0 { (ox - hit.x) * tx } else { (hit.x + 1 - ox) * tx };
    var my = if vy < 0.0 { (oy - hit.y) * ty } else { (hit.y + 1 - oy) * ty };
    var mz = if vz < 0.0 { (oz - hit.z) * tz } else { (hit.z + 1 - oz) * tz };
    if abs(vx) < 0.0000001 { mx = 1.0e30; }
    if abs(vy) < 0.0000001 { my = 1.0e30; }
    if abs(vz) < 0.0000001 { mz = 1.0e30; }
    var distance: f64 = 0.0;
    while distance <= reach {
        if world_get(hit.x, hit.y, hit.z) != AIR { hit.found = true; return hit; }
        hit.previous_x = hit.x;
        hit.previous_y = hit.y;
        hit.previous_z = hit.z;
        if mx <= my && mx <= mz { distance = mx; mx += tx; hit.x += sx; }
        else if my <= mz { distance = my; my += ty; hit.y += sy; }
        else { distance = mz; mz += tz; hit.z += sz; }
    }
    return hit;
}
fn player_target(p: Player) -> RayHit {
    world_raycast(p.x, p.y + EYE_HEIGHT, p.z,
                  sin(p.yaw) * cos(p.pitch), sin(p.pitch), -cos(p.yaw) * cos(p.pitch), 5.0)
}
fn player_break(hit: RayHit) -> bool {
    if !hit.found || world_get(hit.x, hit.y, hit.z) == BEDROCK { return false; }
    return world_set(hit.x, hit.y, hit.z, AIR);
}
fn player_place(p: Player, hit: RayHit, material: u8) -> bool {
    if !hit.found || material == AIR || material >= BEDROCK { return false; }
    let x = hit.previous_x;
    let y = hit.previous_y;
    let z = hit.previous_z;
    if !world_inside(x, y, z) || world_get(x, y, z) != AIR || player_overlaps(p, x, y, z) { return false; }
    return world_set(x, y, z, material);
}
