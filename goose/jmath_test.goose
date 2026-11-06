// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Golden vectors from jmath.rs's own tests (round_f32/round) and the HotSpot-25
// AArch64 (Apple M4) FdLibm results embedded there. Full notice: licenses/upstream-MIT.txt.
import std;
import core.jmath;

fn fl(bits: u64) -> f64 { jmath::gx_f64_from_bits(bits) }
fn bits(x: f64) -> u64 { jmath::gx_f64_bits(x) }

fn main() {
    // Math.round(float): exact, ties toward +inf, saturating, NaN -> 0.
    assert(jmath::round_f32(0.49999997) == 0);
    assert(jmath::round_f32(0.5) == 1);
    assert(jmath::round_f32(-0.5) == 0);
    assert(jmath::round_f32(2.5) == 3);
    assert(jmath::round_f32(-2.5) == -2);
    assert(jmath::round_f32(1.0e10) == 2147483647);
    assert(jmath::round_f32(-1.0e10) == -2147483648);
    assert(jmath::round_f32(jmath::gx_f32_from_bits(0x7fc00000)) == 0); // NaN
    assert(jmath::round_f32(8388609.0) == 8388609);

    // Math.round(double): ties toward +inf, saturation, specials.
    assert(jmath::round(-2.5) == -2);
    assert(jmath::round(2.5) == 3);
    assert(jmath::round(-0.5) == 0);
    assert(jmath::round(-1.5) == -1);
    assert(jmath::round(0.5) == 1);
    assert(jmath::round(-2.6) == -3);
    assert(jmath::round(0.49999999999999994) == 0);
    assert(jmath::round(0.0 / 0.0) == 0);                     // NaN -> 0
    assert(jmath::round(1.0 / 0.0) == 9223372036854775807);   // +inf -> i64::MAX
    assert(jmath::round(-1.0 / 0.0) == -9223372036854775808); // -inf -> i64::MIN

    // FdLibm.log, from HotSpot 25 on an Apple M4 (argument bits, result bits).
    assert(bits(jmath::fdlibm_log(fl(0x40569150fe105e6f))) == 0x401202e102f27b78);
    assert(bits(jmath::fdlibm_log(fl(0x3ff5dec70f518390))) == 0x3fd4009cda1f8856);
    assert(bits(jmath::fdlibm_log(fl(0x3feb3d17325d493e))) == 0xbfc49ee8c4866f84);
    assert(bits(jmath::fdlibm_log(fl(0x3fe735a40edf77ba))) == 0xbfd48e1816b18bf2);

    // FdLibm.log10.
    assert(bits(jmath::fdlibm_log10(fl(0x3ff5dec70f518390))) == 0x3fc15fb52aa5f051);
    assert(bits(jmath::fdlibm_log10(fl(0x3feb3d17325d493e))) == 0xbfb1e933cae02f5f);
    assert(bits(jmath::fdlibm_log10(fl(0x3fe735a40edf77ba))) == 0xbfc1da98cbb8974c);
    assert(bits(jmath::fdlibm_log10(fl(0x3fe7febd9ca1bab0))) == 0xbfc000e8e3b0b56e);

    // FdLibm.cbrt.
    assert(bits(jmath::fdlibm_cbrt(fl(0x401bb21f8568bd0c))) == 0x3ffe7ee30c7a56f9);
    assert(bits(jmath::fdlibm_cbrt(fl(0x3fc59f3a45c8fb80))) == 0x3fe1b0791e0f8a74);
    assert(bits(jmath::fdlibm_cbrt(fl(0xbfdd73cdc693ddab))) == 0xbfe8b4a6f43f1b73);
    assert(bits(jmath::fdlibm_cbrt(fl(0xbfeb0715a8425e10))) == 0xbfee3f99498da6f6);

    // FdLibm.hypot (x bits, y bits, result bits).
    assert(bits(jmath::fdlibm_hypot(fl(0xc0569150fe105e6f), fl(0xc0056b970bf3b41a))) == 0x405693db72f461a6);
    assert(bits(jmath::fdlibm_hypot(fl(0x3fdc5999df9270f4), fl(0xc006c3ad990f72c0))) == 0x400709de26a1e40a);
    assert(bits(jmath::fdlibm_hypot(fl(0x401bb21f8568bd0c), fl(0xbffeb93d1a2ef904))) == 0x401cbdbb623d20c3);
    assert(bits(jmath::fdlibm_hypot(fl(0x4026d31c32ddb50b), fl(0xc00e7b2771d6867c))) == 0x40281027bb313aae);

    // Sign normalization and scaling regressions. These are exact 3-4-5
    // triangles, scaled by powers of two, independent of the implementation.
    assert(bits(jmath::fdlibm_hypot(0.0, -0.0)) == 0);
    assert(bits(jmath::fdlibm_hypot(-0.0, 0.0)) == 0);
    assert(bits(jmath::fdlibm_hypot(fl(0x6588000000000000), fl(0x6590000000000000))) == 0x6594000000000000);
    assert(bits(jmath::fdlibm_hypot(fl(0x1a88000000000000), fl(0x1a90000000000000))) == 0x1a94000000000000);
    assert(bits(jmath::fdlibm_hypot(fl(3), fl(4))) == 5);
    // Rounded real cube root, which differs from this machine's glibc cbrt.
    assert(bits(jmath::cbrt(2.0)) == 0x3ff428a2f98d728b);
    assert(bits(jmath::cbrt(-0.0)) == 0x8000000000000000);

    print("jmath: round_f32/round and FdLibm log/log10/cbrt/hypot golden vectors passed.");
}
