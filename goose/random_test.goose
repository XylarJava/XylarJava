// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Adapted from random.rs's own unit tests (JDK sequence, invalid-bound stream
// preservation, Xoroshiro zero-state fallback). Full notice: licenses/upstream-MIT.txt.
import std;
import core.random;
import core.jmath;

fn main() {
    // java.util.Random(0): the documented JDK sequence and sign-extended nextLong.
    var a = jrandom::new_legacy(0);
    assert(jrandom::legacy_next_int(a) == -1155484576);
    assert(jrandom::legacy_next_int(a) == -723955400);
    var a0 = jrandom::new_legacy(0);
    assert(jrandom::legacy_next_long(a0) == -4962768465676381896);

    // Invalid bounds return Err and consume no state on either stream.
    var l = jrandom::new_legacy(0);
    let lv0, lok0 = jrandom::legacy_next_int_bound(l, 0);
    let lv1, lok1 = jrandom::legacy_next_int_bound(l, -1);
    let lv2, lok2 = jrandom::legacy_next_int_bound(l, -2147483648);
    assert(!lok0 && !lok1 && !lok2);
    jrandom::legacy_consume_count(l, -1);
    var lref = jrandom::new_legacy(0);
    assert(jrandom::legacy_next_long(l) == jrandom::legacy_next_long(lref));

    var x = jrandom::new_xoroshiro(0);
    let xv0, xok0 = jrandom::xoroshiro_next_int_bound(x, 0);
    let xv1, xok1 = jrandom::xoroshiro_next_int_bound(x, -1);
    let xv2, xok2 = jrandom::xoroshiro_next_int_bound(x, -2147483648);
    assert(!xok0 && !xok1 && !xok2);
    jrandom::xoroshiro_consume_count(x, -1);
    var xref = jrandom::new_xoroshiro(0);
    assert(jrandom::xoroshiro_next_long(x) == jrandom::xoroshiro_next_long(xref));

    // Xoroshiro all-zero state falls back to the golden/silver ratio words.
    var z = jrandom::xoroshiro_from_state(0, 0);
    var g = jrandom::xoroshiro_from_state(0x9e3779b97f4a7c15, 0x6a09e667f3bcc909);
    for i in 100 { assert(jrandom::xoroshiro_next_long(z) == jrandom::xoroshiro_next_long(g)); }

    // Seeding is deterministic: two sources from the same seed agree.
    var p = jrandom::new_xoroshiro(-1);
    var q = jrandom::new_xoroshiro(-1);
    for i in 50 { assert(jrandom::xoroshiro_next_long(p) == jrandom::xoroshiro_next_long(q)); }

    // mix_stafford13(0) is 0 (both xor-shift rounds leave zero fixed).
    assert(jrandom::mix_stafford13(0) == 0);

    // A cached Gaussian must not consume another random draw. Reseeding
    // discards that cache; a fork starts with a fresh cache of its own.
    var gl = jrandom::new_legacy(42);
    jrandom::legacy_next_gaussian(gl);
    let legacy_cached = gl.gaussian;
    let legacy_after_pair = gl.state;
    jrandom::legacy_next_int(gl);
    let legacy_after_int = gl.state;
    assert(jmath::gx_f64_bits(jrandom::legacy_next_gaussian(gl)) == jmath::gx_f64_bits(legacy_cached));
    assert(gl.state == legacy_after_int);
    assert(legacy_after_int != legacy_after_pair);
    jrandom::legacy_next_gaussian(gl);
    jrandom::legacy_set_seed(gl, 42);
    var glref = jrandom::new_legacy(42);
    assert(jmath::gx_f64_bits(jrandom::legacy_next_gaussian(gl)) == jmath::gx_f64_bits(jrandom::legacy_next_gaussian(glref)));
    assert(gl.state == glref.state);
    var glfork = jrandom::legacy_fork(gl);
    assert(!glfork.has_gaussian && gl.has_gaussian);

    var gx = jrandom::new_xoroshiro(-42);
    jrandom::xoroshiro_next_gaussian(gx);
    let xoroshiro_cached = gx.gaussian;
    jrandom::xoroshiro_next_int(gx);
    let cached_lo = gx.lo;
    let cached_hi = gx.hi;
    assert(jmath::gx_f64_bits(jrandom::xoroshiro_next_gaussian(gx)) == jmath::gx_f64_bits(xoroshiro_cached));
    assert(gx.lo == cached_lo && gx.hi == cached_hi);
    jrandom::xoroshiro_next_gaussian(gx);
    jrandom::xoroshiro_set_seed(gx, -42);
    var gxref = jrandom::new_xoroshiro(-42);
    assert(jmath::gx_f64_bits(jrandom::xoroshiro_next_gaussian(gx)) == jmath::gx_f64_bits(jrandom::xoroshiro_next_gaussian(gxref)));
    assert(gx.lo == gxref.lo && gx.hi == gxref.hi);
    var gxfork = jrandom::xoroshiro_fork(gx);
    assert(!gxfork.has_gaussian && gx.has_gaussian);

    print("Random streams: JDK LCG sequence, invalid-bound preservation, Xoroshiro fallback, Gaussian cache/reset/fork passed.");
}
