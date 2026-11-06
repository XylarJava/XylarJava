// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Rust-to-Goose adaptation for Xylar Goose. Full notice: ../licenses/upstream-MIT.txt.
// Reference file/revision: ../upstream.lock.json (jmath.rs).
//
// Scope on this x86-64 Linux target:
//   * round_f32 / round  -- jmath's own exact integer rounding (saturating like
//     Rust's `as`, which Goose's float->int cast does not do).
//   * fdlibm_{log,log10,cbrt,hypot} -- StrictMath/FdLibm kernels, verbatim
//     translations of jmath::fdlibm (pure f64 bit arithmetic, no libm).
//   * log/log10/exp/pow/sin/cos/tan/sqrt -- platform-libm entry points;
//     cbrt -- Rust compiler-builtins CORE-MATH (nearest rounding).
//     Compared to the pinned Rust functions on x86-64 Linux by
//     tools/jmath_parity.py. This is not independent HotSpot certification.
//
// NOT ported here (documented pending): the aarch64 sin/cos stub, the Windows
// `exact` double-double sin/cos, atan2 (depends on crate::geometry), and
// atan/asin/acos (Rust's software `libm` crate, which is not the system libm).
import std;
import math;
namespace jmath;

extern fn gx_f64_bits(x: f64) -> u64;
extern fn gx_f64_from_bits(u: u64) -> f64;
extern fn gx_f32_bits(x: f32) -> u32;
extern fn gx_f32_from_bits(u: u32) -> f32;
extern "log" fn platform_log(x: f64) -> f64;
extern "log10" fn platform_log10(x: f64) -> f64;
extern "sin" fn platform_sin(x: f64) -> f64;
extern "cos" fn platform_cos(x: f64) -> f64;
extern "tan" fn platform_tan(x: f64) -> f64;
extern "exp" fn platform_exp(x: f64) -> f64;
extern "pow" fn platform_pow(x: f64, y: f64) -> f64;
extern "fma" fn fused_multiply_add(x: f64, y: f64, z: f64) -> f64;

// --- saturating float -> int, matching Rust's `as` (NaN -> 0, out of range
// clamps to the type bounds). Goose's own float->int cast does not saturate.
fn to_i32_sat(a: f32) -> i32 {
    if a != a { return 0; }
    if a >= 2147483648.0 { return 2147483647; }
    if a <= -2147483648.0 { return -2147483648; }
    return a as! i32;
}
fn to_i64_sat(v: f64) -> i64 {
    if v != v { return 0; }
    if v >= 9223372036854775808.0 { return 9223372036854775807; }
    if v <= -9223372036854775808.0 { return -9223372036854775808; }
    return v as! i64;
}

// Math.round(float): exact (no `x + 0.5` in f32), ties toward +inf, saturating, NaN -> 0.
fn round_f32(a: f32) -> i32 {
    let bits = gx_f32_bits(a) as! i32;
    let biased_exp = (bits & 0x7F800000) >> 23;
    let shift = (22 + 127) - biased_exp;
    if (shift & -32) == 0 {
        var r = (bits & 0x007FFFFF) | 0x00800000;
        if bits < 0 { r = -r; }
        return ((r >> shift) + 1) >> 1;
    }
    return to_i32_sat(a);
}

// Math.round(double): nearest long, ties toward +inf, saturating, NaN -> 0.
fn round(x: f64) -> i64 {
    let f = floor(x);
    return to_i64_sat(if x - f >= 0.5 { f + 1.0 } else { f });
}

// --- FdLibm (StrictMath) kernels, translated verbatim from jmath::fdlibm. ----
let TWO54: f64 = 1.80143985094819840000e+16;

// Rust f64::abs clears the sign bit, so it normalizes -0.0 to +0.0; Goose's
// std abs (x < 0 ? -x : x) leaves -0.0 unchanged. Use the bit-clearing form.
fn fabs(x: f64) -> f64 { gx_f64_from_bits(gx_f64_bits(x) & 0x7fffffffffffffff) }

fn hi(x: f64) -> i32 { (gx_f64_bits(x) >> 32) as! i32 }
fn with_hi(x: f64, high: i32) -> f64 {
    return gx_f64_from_bits((gx_f64_bits(x) & 0xffffffff) | (((high as! u32) as u64) << 32));
}

fn fdlibm_log(x_in: f64) -> f64 {
    let LN2_HI: f64 = 6.93147180369123816490e-01;
    let LN2_LO: f64 = 1.90821492927058770002e-10;
    let LG1: f64 = 6.666666666666735130e-01;
    let LG2: f64 = 3.999999999940941908e-01;
    let LG3: f64 = 2.857142874366239149e-01;
    let LG4: f64 = 2.222219843214978396e-01;
    let LG5: f64 = 1.818357216161805012e-01;
    let LG6: f64 = 1.531383769920937332e-01;
    let LG7: f64 = 1.479819860511658591e-01;
    var x = x_in;
    var hx = hi(x);
    let lx = gx_f64_bits(x) as! u32;
    var k: i32 = 0;
    if hx < 0x00100000 {
        if (((hx & 0x7fffffff) as! u32) | lx) == 0 { return -TWO54 / 0.0; }
        if hx < 0 { return (x - x) / 0.0; }
        k -= 54; x *= TWO54; hx = hi(x);
    }
    if hx >= 0x7ff00000 { return x + x; }
    k += (hx >> 20) - 1023;
    hx &= 0x000fffff;
    let i0 = (hx + 0x95f64) & 0x100000;
    x = with_hi(x, hx | (i0 ^ 0x3ff00000));
    k += i0 >> 20;
    let f = x - 1.0;
    if (0x000fffff & (2 + hx)) < 3 {
        if f == 0.0 {
            if k == 0 { return 0.0; }
            let dk = k as f64;
            return dk * LN2_HI + dk * LN2_LO;
        }
        let r = f * f * (0.5 - 0.33333333333333333 * f);
        if k == 0 { return f - r; }
        let dk = k as f64;
        return dk * LN2_HI - ((r - dk * LN2_LO) - f);
    }
    let s = f / (2.0 + f);
    let dk = k as f64;
    let z = s * s;
    var i = hx - 0x6147a;
    let w = z * z;
    let j = 0x6b851 - hx;
    let t1 = w * (LG2 + w * (LG4 + w * LG6));
    let t2 = z * (LG1 + w * (LG3 + w * (LG5 + w * LG7)));
    i |= j;
    let r = t2 + t1;
    if i > 0 {
        let hfsq = 0.5 * f * f;
        if k == 0 { return f - (hfsq - s * (hfsq + r)); }
        return dk * LN2_HI - ((hfsq - (s * (hfsq + r) + dk * LN2_LO)) - f);
    }
    if k == 0 { return f - s * (f - r); }
    return dk * LN2_HI - ((s * (f - r) - dk * LN2_LO) - f);
}

fn fdlibm_log10(x_in: f64) -> f64 {
    let IVLN10: f64 = 4.34294481903251816668e-01;
    let LOG10_2HI: f64 = 3.01029995663611771306e-01;
    let LOG10_2LO: f64 = 3.69423907715893078616e-13;
    var x = x_in;
    var hx = hi(x);
    let lx = gx_f64_bits(x) as! u32;
    var k: i32 = 0;
    if hx < 0x00100000 {
        if (((hx & 0x7fffffff) as! u32) | lx) == 0 { return -TWO54 / 0.0; }
        if hx < 0 { return (x - x) / 0.0; }
        k -= 54; x *= TWO54; hx = hi(x);
    }
    if hx >= 0x7ff00000 { return x + x; }
    k += (hx >> 20) - 1023;
    let i = ((k as! u32) >> 31) as! i32;
    hx = (hx & 0x000fffff) | ((0x3ff - i) << 20);
    let y = (k + i) as f64;
    x = with_hi(x, hx);
    let z = y * LOG10_2LO + IVLN10 * fdlibm_log(x);
    return z + y * LOG10_2HI;
}

fn fdlibm_cbrt(x_in: f64) -> f64 {
    let B1: i32 = 715094163;
    let B2: i32 = 696219795;
    let C: f64 = 5.42857142857142815906e-01;
    let D: f64 = -7.05306122448979611050e-01;
    let E: f64 = 1.41428571428571436819e+00;
    let F: f64 = 1.60714285714285720630e+00;
    let G: f64 = 3.57142857142857150787e-01;
    var x = x_in;
    if x == 0.0 || !(x == x && (x - x) == 0.0) { return x; }
    let sign = if x < 0.0 { -1.0 } else { 1.0 };
    x = fabs(x);
    var t = 0.0;
    if x < 2.2250738585072014e-308 {
        t = 18014398509481984.0; // 2^54
        t *= x;
        t = with_hi(t, hi(t) / 3 + B2);
    } else {
        t = with_hi(t, hi(x) / 3 + B1);
    }
    var r = t * t / x;
    var s = C + r * t;
    t *= G + F / (s + E + D / s);
    t = gx_f64_from_bits(gx_f64_bits(t) & 0xffffffff00000000);
    t = with_hi(t, hi(t) + 1);
    s = t * t;
    r = x / s;
    let w = t + t;
    r = (r - t) / (w + r);
    t += t * r;
    return sign * t;
}

fn is_finite(x: f64) -> bool { x == x && (x - x) == 0.0 }

fn fdlibm_hypot(x: f64, y: f64) -> f64 {
    var a = fabs(x);
    var b = fabs(y);
    let inf = 1.0 / 0.0;
    if !is_finite(a) || !is_finite(b) {
        if a == inf || b == inf { return inf; }
        return a + b;
    }
    if b > a { let tmp = a; a = b; b = tmp; }
    var ha = hi(a);
    var hb = hi(b);
    if ha - hb > 0x3c00000 { return a + b; }
    var k: i32 = 0;
    if a > gx_f64_from_bits(0x5f300000ffffffff) {
        ha -= 0x25800000;
        hb -= 0x25800000;
        a *= gx_f64_from_bits(0x1a70000000000000); // 2^-600
        b *= gx_f64_from_bits(0x1a70000000000000);
        k += 600;
    }
    if b < gx_f64_from_bits(0x20b0000000000000) { // 2^-500
        if b < 2.2250738585072014e-308 {
            if b == 0.0 { return a; }
            let t1 = gx_f64_from_bits(0x7fd0000000000000); // 2^1022
            b *= t1;
            a *= t1;
            k -= 1022;
        } else {
            ha += 0x25800000;
            hb += 0x25800000;
            a *= gx_f64_from_bits(0x6570000000000000); // 2^600
            b *= gx_f64_from_bits(0x6570000000000000);
            k -= 600;
        }
    }
    var w = a - b;
    if w > b {
        let t1 = with_hi(0.0, ha);
        let t2 = a - t1;
        w = sqrt(t1 * t1 - (b * (-b) - t2 * (a + t1)));
    } else {
        a += a;
        let y1 = with_hi(0.0, hb);
        let y2 = b - y1;
        let t1 = with_hi(0.0, ha + 0x100000);
        let t2 = a - t1;
        w = sqrt(t1 * y1 - (w * (-w) - (t1 * y2 + t2 * b)));
    }
    if k != 0 { return gx_f64_from_bits(((1023 + k) as! u64) << 52) * w; }
    return w;
}

// The pinned Rust entry points on x86-64 Linux. Parity here establishes Rust
// equivalence, not an independent comparison with HotSpot's Intel intrinsics.
fn log(x: f64) -> f64 { platform_log(x) }
fn log10(x: f64) -> f64 { platform_log10(x) }
fn sin(x: f64) -> f64 { platform_sin(x) }
fn cos(x: f64) -> f64 { platform_cos(x) }
fn tan(x: f64) -> f64 { platform_tan(x) }
fn exp(x: f64) -> f64 { platform_exp(x) }
fn pow(x: f64, y: f64) -> f64 { platform_pow(x, y) }
fn hypot(x: f64, y: f64) -> f64 { fdlibm_hypot(x, y) }

// Rust 1.98.1 resolves f64::cbrt to compiler-builtins' CORE-MATH routine,
// rather than glibc cbrt. Nearest-rounding translation of:
// https://github.com/rust-lang/compiler-builtins/blob/main/libm/src/math/cbrt.rs
// Copyright (c) 2021-2022 Alexei Sibidanov. Rust port: Trevor Gross (2025).
// SPDX-License-Identifier: MIT. Full notice: ../licenses/core-math-MIT.txt.
// Arithmetic and bit operations stay in Goose; only IEEE fused multiply-add
// is supplied by libm. Other floating-point rounding modes are not supported.
fn cbrt(x: f64) -> f64 {
    let hx = gx_f64_bits(x);
    var mant = hx & 0x000fffffffffffff;
    let sign = hx >> 63;
    var e = ((hx >> 52) as! u32) & 2047;
    if ((e + 1) & 2047) < 2 {
        let ix = hx & 0x7fffffffffffffff;
        if e == 2047 || ix == 0 { return x + x; }
        // Normalize subnormals, retaining Rust's wrapping exponent arithmetic.
        var nz: i64 = 0;
        var scan = ix;
        while (scan & 0x8000000000000000) == 0 { scan <<= 1; nz += 1; }
        nz -= 11;
        mant = (mant << nz) & 0x000fffffffffffff;
        e -= (nz - 1) as! u32;
    }
    e += 3072;
    let cvt1 = mant | 0x3ff0000000000000;
    let et = e / 3;
    let it = e % 3;
    let zz = gx_f64_from_bits((cvt1 + ((it as u64) << 52)) | (sign << 63));
    let scales: u64[3] = [0x3ff0000000000000, 0x3ff428a2f98d728b, 0x3ff965fea53d6e3d];
    let isc = gx_f64_from_bits(scales[it as i64] | (sign << 63));
    let z = gx_f64_from_bits(cvt1);
    let r = 1.0 / z;
    let rsc: f64[6] = [1.0, -1.0, 0.5, -0.5, 0.25, -0.25];
    let rr = r * rsc[((it as i64) << 1) | (sign as i64)];
    let z2 = z * z;
    let c0 = gx_f64_from_bits(0x3fe1b0babccfef9c) + z * gx_f64_from_bits(0x3fe2c9a3e94d1da5);
    let c2 = gx_f64_from_bits(0xbfc4dc30b1a1ddba) + z * gx_f64_from_bits(0x3f97a8d3e4ec9b07);
    let u0 = gx_f64_from_bits(0x3fd5555555555555);
    let u1 = gx_f64_from_bits(0x3fcc71c71c71c71c);
    var y = c0 + z2 * c2;
    var y2 = y * y;
    var h = y2 * (y * r) - 1.0;
    y -= (h * y) * (u0 - u1 * h);
    y *= isc;
    y2 = y * y;
    var y2l = fused_multiply_add(y, y, -y2);
    var y3 = y2 * y;
    var y3l = fused_multiply_add(y, y2, -y3) + y * y2l;
    h = ((y3 - zz) + y3l) * rr;
    var dy = h * (y * u0);
    var y1 = y - dy;
    dy = (y - y1) - dy;
    let half_ulp = gx_f64_from_bits(0x3ca0000000000000); // 2^-53
    let three_halves = gx_f64_from_bits(0x3cb8000000000000);
    var ady = fabs(dy);
    var ady0 = fabs(ady - half_ulp);
    var ady1 = fabs(ady - three_halves);
    if ady0 < gx_f64_from_bits(0x3b40000000000000) || ady1 < gx_f64_from_bits(0x3b40000000000000) {
        y2 = y1 * y1;
        y2l = fused_multiply_add(y1, y1, -y2);
        y3 = y2 * y1;
        y3l = fused_multiply_add(y1, y2, -y3) + y1 * y2l;
        h = ((y3 - zz) + y3l) * rr;
        dy = h * (y1 * u0);
        y = y1 - dy;
        dy = (y1 - y) - dy;
        y1 = y;
        ady = fabs(dy);
        ady0 = fabs(ady - half_ulp);
        ady1 = fabs(ady - three_halves);
        if ady0 < gx_f64_from_bits(0x39d0000000000000) || ady1 < gx_f64_from_bits(0x39d0000000000000) {
            if fabs(zz) == gx_f64_from_bits(0x4009b78223aa307c) {
                y1 = gx_f64_from_bits(0x3ff79d15d0e8d59c | (sign << 63));
            }
            if fabs(zz) == gx_f64_from_bits(0x401a202bfc89ddff) {
                y1 = gx_f64_from_bits(0x3ffde87aa837820f | (sign << 63));
            }
        }
    }
    var cvt3 = gx_f64_bits(y1) + (((et - 342 - 1023) as u64) << 52);
    let m0 = cvt3 << 30;
    let m1 = m0 >> 63;
    if (m0 ^ m1) <= (1 << 30) {
        let cvt4 = (gx_f64_bits(y1) + (164 << 15)) & 0xffffffffffff0000;
        if fabs((gx_f64_from_bits(cvt4) - y1) - dy) < gx_f64_from_bits(0x3c30000000000000) || fabs(zz) == 1.0 {
            cvt3 = (cvt3 + (1 << 15)) & 0xffffffffffff0000;
        }
    }
    return gx_f64_from_bits(cvt3);
}
