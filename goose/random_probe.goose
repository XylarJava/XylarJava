// Drives the translated integer and floating-point streams from a line-oriented fixture and
// prints one decimal value per emitted draw, for byte-exact comparison with the
// Rust oracle in tools/random_parity.py.
//
// Line formats (space-separated tokens):
//   mix <v>                 print mix_stafford13(v)   (v read as i64, reinterpreted u64)
//   L <seed> <op>...        run ops on a fresh LegacyRandom(seed)
//   X <seed> <op>...        run ops on a fresh XoroshiroRandom(seed)
// Ops: i next_int, l next_long, b next_boolean(0/1), s state words,
//      B:<bound> next_int_bound, R:<o>:<b> next_int_between,
//      N:<min>:<max> next_int_between_inclusive, C:<n> consume_count, F fork+next_long.
// Floating ops: f float, d double, g Gaussian, Fg fork+two Gaussians+state,
// S:<seed> reseed, T:<f64bits>:<f64bits> triangle, t:<f32bits>:<f32bits> triangle_float.
// next_int_bound / between* print "E" when the reference returns Err(InvalidBound).
import std;
import os;
import core.random;
import core.jmath;

fn run_legacy(r: jrandom::LegacyRandom&, op: u8[:]) {
    let parts = split(op, ':');
    let tag = op[0];
    if tag == 'i' { print(jrandom::legacy_next_int(r)); }
    else if tag == 'l' { print(jrandom::legacy_next_long(r)); }
    else if tag == 'b' { print(if jrandom::legacy_next_boolean(r) { 1 } else { 0 }); }
    else if op == "f" { print(jmath::gx_f32_bits(jrandom::legacy_next_float(r))); }
    else if op == "d" { print(jmath::gx_f64_bits(jrandom::legacy_next_double(r))); }
    else if op == "g" { print(jmath::gx_f64_bits(jrandom::legacy_next_gaussian(r))); }
    else if op == "Fg" {
        var f = jrandom::legacy_fork(r);
        print(jmath::gx_f64_bits(jrandom::legacy_next_gaussian(f)));
        print(jmath::gx_f64_bits(jrandom::legacy_next_gaussian(f)));
        print(jrandom::legacy_state(f) as! i64);
    }
    else if tag == 'S' {
        let seed, ok = parse_int(parts[1]); assert(ok);
        jrandom::legacy_set_seed(r, seed);
    }
    else if tag == 'T' || tag == 't' {
        let a, ok1 = parse_int(parts[1]); let b, ok2 = parse_int(parts[2]); assert(ok1 && ok2);
        if tag == 'T' {
            print(jmath::gx_f64_bits(jrandom::legacy_triangle(r, jmath::gx_f64_from_bits(a as! u64), jmath::gx_f64_from_bits(b as! u64))));
        } else {
            print(jmath::gx_f32_bits(jrandom::legacy_triangle_float(r, jmath::gx_f32_from_bits(a as! u32), jmath::gx_f32_from_bits(b as! u32))));
        }
    }
    else if tag == 's' { print(jrandom::legacy_state(r) as! i64); }
    else if tag == 'F' { var f = jrandom::legacy_fork(r); print(jrandom::legacy_next_long(f)); }
    else if tag == 'B' {
        let bound, ok1 = parse_int(parts[1]); assert(ok1);
        let value, ok = jrandom::legacy_next_int_bound(r, bound as! i32);
        if ok { print(value); } else { print("E"); }
    } else if tag == 'R' {
        let o, ok1 = parse_int(parts[1]); let b, ok2 = parse_int(parts[2]); assert(ok1 && ok2);
        let value, ok = jrandom::legacy_next_int_between(r, o as! i32, b as! i32);
        if ok { print(value); } else { print("E"); }
    } else if tag == 'N' {
        let mn, ok1 = parse_int(parts[1]); let mx, ok2 = parse_int(parts[2]); assert(ok1 && ok2);
        let value, ok = jrandom::legacy_next_int_between_inclusive(r, mn as! i32, mx as! i32);
        if ok { print(value); } else { print("E"); }
    } else if tag == 'C' {
        let n, ok1 = parse_int(parts[1]); assert(ok1);
        jrandom::legacy_consume_count(r, n as! i32);
    } else { assert(false); }
}

fn run_xoroshiro(r: jrandom::XoroshiroRandom&, op: u8[:]) {
    let parts = split(op, ':');
    let tag = op[0];
    if tag == 'i' { print(jrandom::xoroshiro_next_int(r)); }
    else if tag == 'l' { print(jrandom::xoroshiro_next_long(r)); }
    else if tag == 'b' { print(if jrandom::xoroshiro_next_boolean(r) { 1 } else { 0 }); }
    else if op == "f" { print(jmath::gx_f32_bits(jrandom::xoroshiro_next_float(r))); }
    else if op == "d" { print(jmath::gx_f64_bits(jrandom::xoroshiro_next_double(r))); }
    else if op == "g" { print(jmath::gx_f64_bits(jrandom::xoroshiro_next_gaussian(r))); }
    else if op == "Fg" {
        var f = jrandom::xoroshiro_fork(r);
        print(jmath::gx_f64_bits(jrandom::xoroshiro_next_gaussian(f)));
        print(jmath::gx_f64_bits(jrandom::xoroshiro_next_gaussian(f)));
        print(f.lo as! i64); print(f.hi as! i64);
    }
    else if tag == 'S' {
        let seed, ok = parse_int(parts[1]); assert(ok);
        jrandom::xoroshiro_set_seed(r, seed);
    }
    else if tag == 'T' || tag == 't' {
        let a, ok1 = parse_int(parts[1]); let b, ok2 = parse_int(parts[2]); assert(ok1 && ok2);
        if tag == 'T' {
            print(jmath::gx_f64_bits(jrandom::xoroshiro_triangle(r, jmath::gx_f64_from_bits(a as! u64), jmath::gx_f64_from_bits(b as! u64))));
        } else {
            print(jmath::gx_f32_bits(jrandom::xoroshiro_triangle_float(r, jmath::gx_f32_from_bits(a as! u32), jmath::gx_f32_from_bits(b as! u32))));
        }
    }
    else if tag == 's' { print(r.lo as! i64); print(r.hi as! i64); }
    else if tag == 'F' { var f = jrandom::xoroshiro_fork(r); print(jrandom::xoroshiro_next_long(f)); }
    else if tag == 'B' {
        let bound, ok1 = parse_int(parts[1]); assert(ok1);
        let value, ok = jrandom::xoroshiro_next_int_bound(r, bound as! i32);
        if ok { print(value); } else { print("E"); }
    } else if tag == 'R' {
        let o, ok1 = parse_int(parts[1]); let b, ok2 = parse_int(parts[2]); assert(ok1 && ok2);
        let value, ok = jrandom::xoroshiro_next_int_between(r, o as! i32, b as! i32);
        if ok { print(value); } else { print("E"); }
    } else if tag == 'N' {
        let mn, ok1 = parse_int(parts[1]); let mx, ok2 = parse_int(parts[2]); assert(ok1 && ok2);
        let value, ok = jrandom::xoroshiro_next_int_between_inclusive(r, mn as! i32, mx as! i32);
        if ok { print(value); } else { print("E"); }
    } else if tag == 'C' {
        let n, ok1 = parse_int(parts[1]); assert(ok1);
        jrandom::xoroshiro_consume_count(r, n as! i32);
    } else { assert(false); }
}

fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let tokens = split(line, ' ');
        let kind = tokens[0];
        if kind[0] == 'm' {
            let v, ok = parse_int(tokens[1]); assert(ok);
            print(jrandom::mix_stafford13(v as! u64) as! i64);
            continue;
        }
        let seed, ok = parse_int(tokens[1]); assert(ok);
        if kind[0] == 'L' {
            var r = jrandom::new_legacy(seed);
            for i in 2..tokens.len { run_legacy(r, tokens[i]); }
        } else {
            var r = jrandom::new_xoroshiro(seed);
            for i in 2..tokens.len { run_xoroshiro(r, tokens[i]); }
        }
    }
}
