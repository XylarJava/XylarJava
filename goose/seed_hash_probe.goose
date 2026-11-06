import std;
import os;
import core.seed_hash;
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let seed, ok = parse_int(line);
        assert(ok);
        print(biome_seed::hash(seed));
    }
}
