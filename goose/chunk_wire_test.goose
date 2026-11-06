import std;
import net.chunk;
fn main() {
    var values: i32[4096] = [0; 4096];
    for i in 4096 { values[i] = (i % 20) as i32; }
    var p = chunk_wire::from_values(values, true, 16);
    assert(p.bits == 5 && p.data.len == 342);
    for i in [0, 11, 12, 13, 4095] { assert(chunk_wire::get(p, i) == values[i]); }
    var raw: u8[>..] = [];
    chunk_wire::write_paletted(p, raw);
    var r = wire::reader(raw);
    let back, error = chunk_wire::read_paletted(r, true, 16);
    assert(wire::is_ok(error) && wire::remaining(r) == 0);
    assert(back.bits == p.bits && back.palette == p.palette && back.data == p.data);
    let bios: i32[64] = [2; 64];
    let b = chunk_wire::from_values(bios, false, 7);
    let s = chunk_wire::Section { non_empty: 256, fluids: 0, states: copy(p), biomes: copy(b) };
    var encoded: u8[>..] = [];
    chunk_wire::write_section(s, encoded);
    var reader = wire::reader(encoded);
    let section, result = chunk_wire::read_section(reader, 16, 7);
    assert(wire::is_ok(result) && reader.pos == encoded.len);
    assert(section.non_empty == 256 && section.fluids == 0);
    assert(chunk_wire::get(section.states, 4095) == 15 && chunk_wire::get(section.biomes, 10) == 2);
    let invalid: u8[1] = [33];
    var bad_reader = wire::reader(invalid);
    let bad, failure = chunk_wire::read_paletted(bad_reader, true, 16);
    assert(!wire::is_ok(failure) && bad_reader.pos == 1);
    let sparse: i32[5] = [-1, 1000001, -1, 2000002, 1000001];
    let q = chunk_wire::from_values(sparse, true, 32);
    assert(q.bits == 4 && q.palette.len == 3);
    for i in sparse.len { assert(chunk_wire::get(q, i) == sparse[i]); }
    assert(chunk_wire::get(q, 100000) == -1); // missing word reads raw id zero
    print("Chunk wire: padded longs, 26.3 section counters, block/biome round trip and sparse ids passed.");
}
