// Drives the translated jmath functions from a line-oriented fixture for
// byte-exact comparison with the Rust oracle (tools/jmath_parity.py).
//
// Bit patterns arrive as signed-i64 decimals (reinterpreted to the raw bits);
// float results are printed as unsigned u64 bit patterns, rounds as decimals.
// Ops: rf <f32bits> round_f32 | rd <f64bits> round |
//      log/log10/exp/sin/cos/tan/sqrt/cbrt <f64bits> entry points |
//      pow <f64bits> <f64bits> | flog/flog10/fcbrt <f64bits> fdlibm |
//      fhyp <f64bits> <f64bits> fdlibm hypot.
import std;
import os;
import math;
import core.jmath;

fn f64arg(tok: u8[:]) -> f64 {
    let v, ok = parse_int(tok); assert(ok);
    return jmath::gx_f64_from_bits(v as! u64);
}

fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let p = split(line, ' ');
        let op = p[0];
        if op == "rf" {
            let v, ok = parse_int(p[1]); assert(ok);
            print(jmath::round_f32(jmath::gx_f32_from_bits(v as! u32)));
        } else if op == "rd" {
            print(jmath::round(f64arg(p[1])));
        } else if op == "log" {
            print(jmath::gx_f64_bits(jmath::log(f64arg(p[1]))));
        } else if op == "log10" {
            print(jmath::gx_f64_bits(jmath::log10(f64arg(p[1]))));
        } else if op == "exp" {
            print(jmath::gx_f64_bits(jmath::exp(f64arg(p[1]))));
        } else if op == "sin" {
            print(jmath::gx_f64_bits(jmath::sin(f64arg(p[1]))));
        } else if op == "cos" {
            print(jmath::gx_f64_bits(jmath::cos(f64arg(p[1]))));
        } else if op == "tan" {
            print(jmath::gx_f64_bits(jmath::tan(f64arg(p[1]))));
        } else if op == "sqrt" {
            print(jmath::gx_f64_bits(sqrt(f64arg(p[1]))));
        } else if op == "cbrt" {
            print(jmath::gx_f64_bits(jmath::cbrt(f64arg(p[1]))));
        } else if op == "pow" {
            print(jmath::gx_f64_bits(jmath::pow(f64arg(p[1]), f64arg(p[2]))));
        } else if op == "flog" {
            print(jmath::gx_f64_bits(jmath::fdlibm_log(f64arg(p[1]))));
        } else if op == "flog10" {
            print(jmath::gx_f64_bits(jmath::fdlibm_log10(f64arg(p[1]))));
        } else if op == "fcbrt" {
            print(jmath::gx_f64_bits(jmath::fdlibm_cbrt(f64arg(p[1]))));
        } else if op == "fhyp" {
            print(jmath::gx_f64_bits(jmath::fdlibm_hypot(f64arg(p[1]), f64arg(p[2]))));
        } else {
            assert(false);
        }
    }
}
