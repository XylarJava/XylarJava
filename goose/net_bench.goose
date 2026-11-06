// Reproducible CPU kernel benchmark; not a game/FPS benchmark.
import std;
import os;
import net.chunk;
fn exercise(values: i32[:], global_bits: u8) -> u64 {
    let p = chunk_wire::from_values(values, true, global_bits);
    var checksum: u64 = (p.bits as u64) + (p.palette.len as u64) + (p.data.len as u64);
    for value in p.palette { checksum += value as! u32; }
    for i in p.data.len { checksum += (p.data[i] as! u64) ^ (i as u64); }
    return checksum;
}
fn main() {
    let arguments = args();
    assert(arguments.len == 3);
    let kind, a = parse_int(arguments[1]);
    let rounds, b = parse_int(arguments[2]);
    assert(a && b && kind >= 0 && kind <= 2 && rounds >= 1 && rounds <= 10000);
    let global_bits: u8 = if kind == 2 { 32 } else { 16 };
    var values: i32[4096] = [0; 4096];
    for i in values.len { values[i] = (if kind == 0 { i % 20 } else if kind == 1 { i * 7 } else { 1000000 + i * 31 }) as i32; }
    var checksum: u64 = 0;
    for warmup in 4 { checksum += exercise(values, global_bits); }
    let start = clock_ns();
    for round in rounds {
        values[0] = (if kind == 0 { round % 20 } else if kind == 1 { (round * 17) % 30000 } else { 1000000 + round * 100003 }) as i32;
        checksum += exercise(values, global_bits);
    }
    let elapsed = clock_ns() - start;
    print(checksum, " ", elapsed);
}
