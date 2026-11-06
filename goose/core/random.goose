// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Rust-to-Goose adaptation for Xylar Goose. Full notice: ../licenses/upstream-MIT.txt.
// Reference file/revision: ../upstream.lock.json (random.rs).
//
// Scope: LegacyRandom, XoroshiroRandom, their integer/float/double draws,
// cached MarsagliaPolar Gaussians, triangle/triangle_float and reseeding.
// Positional/worldgen/hash submodules remain pending. tools/random_parity.py
// compares raw float bits and mixed draw/state sequences with the pinned Rust.
import math;
import core.jmath;
namespace jrandom;

// java.util.Random LCG constants.
let LEGACY_MULTIPLIER: u64 = 0x5deece66d;
let LEGACY_INCREMENT: u64 = 11;
let LEGACY_MASK: u64 = (1 << 48) - 1;
// Xoroshiro128++ seeding constants (Xoroshiro128PlusPlus / RandomSupport).
let GOLDEN_RATIO: u64 = 0x9e3779b97f4a7c15;
let SILVER_RATIO: u64 = 0x6a09e667f3bcc909;

// Option<f64> becomes a presence bit and a cached second value.
struct LegacyRandom { state: u64 = 0, gaussian: f64 = 0.0, has_gaussian: bool = false }
struct XoroshiroRandom { lo: u64 = 0, hi: u64 = 0, gaussian: f64 = 0.0, has_gaussian: bool = false }

fn new_legacy(seed: i64) -> LegacyRandom {
    return LegacyRandom { state: ((seed as! u64) ^ LEGACY_MULTIPLIER) & LEGACY_MASK };
}
// Construct from an already-masked 48-bit state (matches LegacyRandom::from_state).
fn legacy_from_state(state: u64) -> LegacyRandom { LegacyRandom { state } }
fn legacy_state(r: LegacyRandom&) -> u64 { r.state }

// Advance the LCG and take the top `count` bits, reinterpreted as the signed
// 32-bit result Java's Random.next(int) returns.
fn legacy_bits(r: LegacyRandom&, count: i64) -> i32 {
    r.state = (r.state * LEGACY_MULTIPLIER + LEGACY_INCREMENT) & LEGACY_MASK;
    return (r.state >> (48 - count)) as! i32;
}

fn legacy_next_int(r: LegacyRandom&) -> i32 { legacy_bits(r, 32) }

// Returns (value, ok); ok=false is the reference Err(InvalidBound).
fn legacy_next_int_bound(r: LegacyRandom&, bound: i32) -> i32, bool {
    if bound <= 0 { return 0, false; }
    if (bound & (bound - 1)) == 0 {
        let b = legacy_bits(r, 31);
        return (((bound as i64) * b) >> 31) as! i32, true;
    }
    while true {
        let b = legacy_bits(r, 31);
        let value = b % bound;
        // Java's signed overflow is part of the rejection rule; the sum is
        // truncated to i32 before the sign test.
        let w = ((b as i64) - value + (bound - 1)) as! i32;
        if w >= 0 { return value, true; }
    }
    return 0, false;
}

fn legacy_next_long(r: LegacyRandom&) -> i64 {
    let high = (legacy_bits(r, 32) as i64) << 32;
    // Java sign-extends the low int before the (wrapping) addition.
    let low = legacy_bits(r, 32) as i64;
    return ((high as! u64) + (low as! u64)) as! i64;
}

fn legacy_next_boolean(r: LegacyRandom&) -> bool { legacy_bits(r, 1) != 0 }

fn legacy_next_int_between_inclusive(r: LegacyRandom&, min: i32, max: i32) -> i32, bool {
    let span = (((max as! u32) - (min as! u32)) + 1) as! i32;
    let value, ok = legacy_next_int_bound(r, span);
    if !ok { return 0, false; }
    return ((value as! u32) + (min as! u32)) as! i32, true;
}

fn legacy_next_int_between(r: LegacyRandom&, origin: i32, bound: i32) -> i32, bool {
    if origin >= bound { return 0, false; }
    let span = ((bound as! u32) - (origin as! u32)) as! i32;
    let value, ok = legacy_next_int_bound(r, span);
    if !ok { return 0, false; }
    return ((value as! u32) + (origin as! u32)) as! i32, true;
}

fn legacy_consume_count(r: LegacyRandom&, count: i32) {
    var k: i32 = 0;
    while k < count { legacy_next_int(r); k += 1; }
}

fn legacy_fork(r: LegacyRandom&) -> LegacyRandom { new_legacy(legacy_next_long(r)) }

// --- Xoroshiro128++ -------------------------------------------------------

fn rotl(value: u64, amount: i64) -> u64 { (value << amount) | (value >> (64 - amount)) }

fn mix_stafford13(value: u64) -> u64 {
    var v = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9;
    v = (v ^ (v >> 27)) * 0x94d049bb133111eb;
    return v ^ (v >> 31);
}

fn xoroshiro_from_state(lo: u64, hi: u64) -> XoroshiroRandom {
    if (lo | hi) == 0 { return XoroshiroRandom { lo: GOLDEN_RATIO, hi: SILVER_RATIO }; }
    return XoroshiroRandom { lo, hi };
}

fn new_xoroshiro(seed: i64) -> XoroshiroRandom {
    let lo = (seed as! u64) ^ SILVER_RATIO;
    return xoroshiro_from_state(mix_stafford13(lo), mix_stafford13(lo + GOLDEN_RATIO));
}

fn xoroshiro_next_long(r: XoroshiroRandom&) -> i64 {
    let lo = r.lo;
    let hi = r.hi ^ lo;
    let result = rotl(lo + r.hi, 17) + lo;
    r.lo = rotl(lo, 49) ^ hi ^ (hi << 21);
    r.hi = rotl(hi, 28);
    return result as! i64;
}

fn xoroshiro_next_int(r: XoroshiroRandom&) -> i32 { xoroshiro_next_long(r) as! i32 }

fn xoroshiro_next_int_bound(r: XoroshiroRandom&, bound: i32) -> i32, bool {
    if bound <= 0 { return 0, false; }
    let b = bound as! u32;
    // wrapping_neg() % bound: (0 - b) wraps to 2^32 - b.
    let threshold = (0 - b) % b;
    while true {
        let product = ((xoroshiro_next_int(r) as! u32) as u64) * b;
        if (product as! u32) >= threshold { return (product >> 32) as! i32, true; }
    }
    return 0, false;
}

fn xoroshiro_next_boolean(r: XoroshiroRandom&) -> bool { (xoroshiro_next_long(r) & 1) != 0 }

fn xoroshiro_next_int_between_inclusive(r: XoroshiroRandom&, min: i32, max: i32) -> i32, bool {
    let span = (((max as! u32) - (min as! u32)) + 1) as! i32;
    let value, ok = xoroshiro_next_int_bound(r, span);
    if !ok { return 0, false; }
    return ((value as! u32) + (min as! u32)) as! i32, true;
}

fn xoroshiro_next_int_between(r: XoroshiroRandom&, origin: i32, bound: i32) -> i32, bool {
    if origin >= bound { return 0, false; }
    let span = ((bound as! u32) - (origin as! u32)) as! i32;
    let value, ok = xoroshiro_next_int_bound(r, span);
    if !ok { return 0, false; }
    return ((value as! u32) + (origin as! u32)) as! i32, true;
}

fn xoroshiro_consume_count(r: XoroshiroRandom&, count: i32) {
    var k: i32 = 0;
    while k < count { xoroshiro_next_int(r); k += 1; }
}

fn xoroshiro_fork(r: XoroshiroRandom&) -> XoroshiroRandom {
    let lo = xoroshiro_next_long(r) as! u64;
    let hi = xoroshiro_next_long(r) as! u64;
    return xoroshiro_from_state(lo, hi);
}

fn legacy_set_seed(r: LegacyRandom&, seed: i64) { r = new_legacy(seed); }

fn legacy_next_float(r: LegacyRandom&) -> f32 { return (legacy_bits(r, 24) as f32) * (1.0 / 16777216.0); }
fn legacy_next_double(r: LegacyRandom&) -> f64 {
    let first = legacy_bits(r, 26) as i64;
    let second = legacy_bits(r, 27) as i64;
    return (((first << 27) + second) as f64) * (1.0 / 9007199254740992.0);
}

// MarsagliaPolarGaussian.nextGaussian, specialized to this source. Keeping the
// draws in separate statements preserves the reference's evaluation order.
fn legacy_next_gaussian(r: LegacyRandom&) -> f64 {
    if r.has_gaussian { r.has_gaussian = false; return r.gaussian; }
    while true {
        let x = 2.0 * legacy_next_double(r) - 1.0;
        let y = 2.0 * legacy_next_double(r) - 1.0;
        let radius = x * x + y * y;
        if radius >= 1.0 || radius == 0.0 { continue; }
        let multiplier = sqrt(-2.0 * jmath::log(radius) / radius);
        r.gaussian = y * multiplier;
        r.has_gaussian = true;
        return x * multiplier;
    }
    return 0.0;
}

fn legacy_triangle(r: LegacyRandom&, mean: f64, deviation: f64) -> f64 {
    let first = legacy_next_double(r);
    let second = legacy_next_double(r);
    return mean + deviation * (first - second);
}

fn legacy_triangle_float(r: LegacyRandom&, mean: f32, deviation: f32) -> f32 {
    let first = legacy_next_float(r);
    let second = legacy_next_float(r);
    return mean + deviation * (first - second);
}

fn xoroshiro_set_seed(r: XoroshiroRandom&, seed: i64) { r = new_xoroshiro(seed); }

fn xoroshiro_next_float(r: XoroshiroRandom&) -> f32 { return (((xoroshiro_next_long(r) as! u64) >> 40) as f32) * (1.0 / 16777216.0); }
fn xoroshiro_next_double(r: XoroshiroRandom&) -> f64 {
    return (((xoroshiro_next_long(r) as! u64) >> 11) as f64) * (1.0 / 9007199254740992.0);
}

// MarsagliaPolarGaussian.nextGaussian, specialized to this source. Keeping the
// draws in separate statements preserves the reference's evaluation order.
fn xoroshiro_next_gaussian(r: XoroshiroRandom&) -> f64 {
    if r.has_gaussian { r.has_gaussian = false; return r.gaussian; }
    while true {
        let x = 2.0 * xoroshiro_next_double(r) - 1.0;
        let y = 2.0 * xoroshiro_next_double(r) - 1.0;
        let radius = x * x + y * y;
        if radius >= 1.0 || radius == 0.0 { continue; }
        let multiplier = sqrt(-2.0 * jmath::log(radius) / radius);
        r.gaussian = y * multiplier;
        r.has_gaussian = true;
        return x * multiplier;
    }
    return 0.0;
}

fn xoroshiro_triangle(r: XoroshiroRandom&, mean: f64, deviation: f64) -> f64 {
    let first = xoroshiro_next_double(r);
    let second = xoroshiro_next_double(r);
    return mean + deviation * (first - second);
}

fn xoroshiro_triangle_float(r: XoroshiroRandom&, mean: f32, deviation: f32) -> f32 {
    let first = xoroshiro_next_float(r);
    let second = xoroshiro_next_float(r);
    return mean + deviation * (first - second);
}
