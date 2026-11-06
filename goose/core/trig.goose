// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// geometry/trig.rs sine-table subset; inverse trigonometry remains pending.
import std;
import core.jmath;
namespace trig263;
let SCALE: f64 = 10430.378350470453;
// Goose globals are worker-local. Each worker initializes its own table once.
var ready = false;
var sine: f32[65536] = [0.0;65536];
fn initialize() {
    if ready { return; }
    for i in 65536 { sine[i] = jmath::sin((i as f64) / SCALE) as f32; }
    ready = true;
}
fn table(index: i64) -> f32 { initialize(); return sine[index]; }
fn sin(angle: f64) -> f32 { initialize(); return sine[jmath::to_i64_sat(angle * SCALE) & 65535]; }
fn cos(angle: f64) -> f32 { initialize(); return sine[jmath::to_i64_sat(angle * SCALE + 16384.0) & 65535]; }
struct Vec3 { x: f64 = 0.0, y: f64 = 0.0, z: f64 = 0.0 }
fn rotate_x(v: Vec3, angle: f32) -> Vec3 {
    let c = cos(angle as f64) as f64; let s = sin(angle as f64) as f64;
    return Vec3 { x: v.x, y: v.y*c + v.z*s, z: v.z*c - v.y*s };
}
fn rotate_y(v: Vec3, angle: f32) -> Vec3 {
    let c = cos(angle as f64) as f64; let s = sin(angle as f64) as f64;
    return Vec3 { x: v.x*c + v.z*s, y: v.y, z: v.z*c - v.x*s };
}
