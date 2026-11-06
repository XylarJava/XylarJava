// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Full protocol chunk.rs port. See ../licenses/upstream-MIT.txt.
import std;
import core.paletted;
import net.buf;
namespace chunk_wire;
let BLOCKS: i64 = 4096;
let BIOMES: i64 = 64;
struct Paletted { bits: u8 = 0, palette: i32[] = [], data: i64[] = [] }
struct Section {
    non_empty: i16 = 0, fluids: i16 = 0,
    states: Paletted = Paletted {}, biomes: Paletted = Paletted {},
}
fn entries(is_blocks: bool) -> i64 { if is_blocks { BLOCKS } else { BIOMES } }
fn configuration(is_blocks: bool, bits: u8, global_bits: u8) -> bool, u8 {
    if bits == 0 { return true, 0; }
    if is_blocks && bits <= 8 { return true, max(4, bits); }
    if !is_blocks && bits <= 3 { return true, bits; }
    return false, max(global_bits, bits);
}
fn storage_len(bits: u8, count: i64) -> i64 {
    if bits == 0 { return 0; }
    let per = 64 / (bits as i64);
    return count / per + (if count % per != 0 { 1 } else { 0 });
}
fn storage_get(data: i64[:], bits: u8, index: i64) -> u32 {
    assert(index >= 0);
    if bits == 0 { return 0; }
    let per = 64 / (bits as i64);
    let cell = index / per;
    if cell >= data.len { return 0; }
    let offset = (index % per) * bits;
    return (((data[cell] as! u64) >> offset) & (((1 as u64) << bits) - 1)) as u32;
}
fn read_paletted(r: wire::Reader&, is_blocks: bool, global_bits: u8) -> Paletted, wire::Error {
    let leading, error = wire::read_u8(r);
    if !wire::is_ok(error) { return Paletted {}, error; }
    let has_palette, bits = configuration(is_blocks, leading, global_bits);
    if bits > 32 { return Paletted {}, wire::Error.InvalidBits { bits: bits }; }
    var palette: i32[>..] = [];
    if has_palette {
        if bits == 0 {
            let value, result = wire::read_varint(r);
            if !wire::is_ok(result) { return Paletted {}, result; }
            palette.push(value);
        } else {
            let count, result = wire::read_count(r, (1 as i64) << bits);
            if !wire::is_ok(result) { return Paletted {}, result; }
            for i in count {
                let value, item_error = wire::read_varint(r);
                if !wire::is_ok(item_error) { return Paletted {}, item_error; }
                palette.push(value);
            }
        }
    }
    let words, words_error = wire::read_fixed_longs(r, storage_len(bits, entries(is_blocks)));
    if !wire::is_ok(words_error) { return Paletted {}, words_error; }
    return Paletted { bits: bits, palette: palette[..], data: words[..] }, wire::Error.Ok;
}
fn write_paletted(p: Paletted&, output: u8[>..]&) {
    wire::write_u8(output, p.bits);
    if p.palette.len != 0 {
        if p.bits != 0 { wire::write_varint(output, p.palette.len as! i32); }
        for value in p.palette { wire::write_varint(output, value); }
    }
    for word in p.data { wire::write_i64(output, word); }
}
fn get(p: Paletted&, index: i64) -> i32 {
    if p.bits == 0 { return if p.palette.len == 0 { 0 } else { p.palette[0] }; }
    let value = storage_get(p.data, p.bits, index);
    if p.palette.len == 0 { return value as! i32; }
    return if value < p.palette.len { p.palette[value] } else { 0 };
}
fn from_palette(palette: i32[:], ids: u32[:], is_blocks: bool, global_bits: u8) -> Paletted {
    let needed = paletted::ceil_log2(palette.len);
    let has_palette, bits = configuration(is_blocks, needed, global_bits);
    if bits == 0 {
        let single: i32[1] = [if palette.len == 0 { 0 } else { palette[0] }];
        return Paletted { bits: 0, palette: single[..] };
    }
    assert(bits <= 32);
    var words: i64[>..] = [];
    let per = 64 / (bits as i64);
    var cursor: i64 = 0;
    while cursor < ids.len {
        var word: u64 = 0;
        for field in min(per, ids.len - cursor) {
            let id: u64 = if has_palette { ids[cursor + field] } else { palette[ids[cursor + field]] as! u64 };
            word |= id << (field * bits);
        }
        words.push(word as! i64);
        cursor += per;
    }
    return Paletted { bits: bits, palette: if has_palette { palette } else { [] }, data: words[..] };
}
// Preserve the fast path's first-occurrence ids. Worker-local storage uses
// zero as "unseen" and id+1 otherwise, so growth initializes only new cells.
var value_index: u32[>..] = [];
fn from_values(values: i32[:], is_blocks: bool, global_bits: u8) -> Paletted {
    var maximum: i32 = 0;
    var dense = true;
    for value in values {
        if value < 0 { dense = false; }
        if value > maximum { maximum = value; }
    }
    if maximum >= 1 << 20 { dense = false; }
    var palette: i32[>..] = [];
    var ids: u32[>..] = [];
    if dense {
        while value_index.len <= maximum { value_index.push(0); }
        var last: i32 = -1;
        var last_id: u32 = 0;
        for value in values {
            if value != last {
                var slot = value_index[value];
                if slot == 0 {
                    palette.push(value);
                    slot = palette.len as u32;
                    value_index[value] = slot;
                }
                last = value; last_id = slot - 1;
            }
            ids.push(last_id);
        }
        for value in palette { value_index[value] = 0; }
    } else {
        // Reference fallback scans its growing list. Use the already-verified
        // first-occurrence dictionary instead; signed/u32 conversion is bijective.
        var unsigned: u32[>..] = [];
        for value in values { unsigned.push(value as! u32); }
        let indexed = paletted::palette_ids(unsigned);
        for value in indexed.palette { palette.push(value as! i32); }
        for id in indexed.ids { ids.push(id); }
    }
    return from_palette(palette, ids, is_blocks, global_bits);
}
fn read_section(r: wire::Reader&, block_bits: u8, biome_bits: u8) -> Section, wire::Error {
    let non_empty, first_error = wire::read_i16(r);
    if !wire::is_ok(first_error) { return Section {}, first_error; }
    let fluids, second_error = wire::read_i16(r);
    if !wire::is_ok(second_error) { return Section {}, second_error; }
    let states, states_error = read_paletted(r, true, block_bits);
    if !wire::is_ok(states_error) { return Section {}, states_error; }
    let biomes, biomes_error = read_paletted(r, false, biome_bits);
    if !wire::is_ok(biomes_error) { return Section {}, biomes_error; }
    return Section { non_empty: non_empty, fluids: fluids, states: copy(states), biomes: copy(biomes) }, wire::Error.Ok;
}
fn write_section(section: Section&, output: u8[>..]&) {
    wire::write_i16(output, section.non_empty);
    wire::write_i16(output, section.fluids);
    write_paletted(section.states, output);
    write_paletted(section.biomes, output);
}

// ClientboundLevelChunkPacketData.BlockEntityInfo; NBT owns canonical bytes.
struct BlockEntityInfo { packed_xz: i8 = 0, y: i16 = 0, kind: i32 = 0, tag: u8[] = [0] }
struct Heightmap { kind: i32 = 0, data: i64[] = [] }
struct ChunkData { heightmaps: Heightmap[] = [], buffer: u8[] = [], block_entities: BlockEntityInfo[] = [] }
struct Layer { data: u8[] = [] }
struct LightData { sky_mask: u8[] = [], block_mask: u8[] = [], empty_sky_mask: u8[] = [], empty_block_mask: u8[] = [], sky: Layer[] = [], block_light: Layer[] = [] }
struct SectionLight { has_sky: bool = false, has_block: bool = false, sky: u8[] = [], block_light: u8[] = [] }
fn read_chunk(r: wire::Reader&) -> ChunkData, wire::Error {
    let count, error = wire::read_count(r, 16);
    if !wire::is_ok(error) { return ChunkData {}, error; }
    var heights: Heightmap[>..] = [];
    for i in count {
        let kind, e = wire::read_varint(r); if !wire::is_ok(e) { return ChunkData {}, e; }
        let words, e2 = wire::read_long_array(r); if !wire::is_ok(e2) { return ChunkData {}, e2; }
        heights.push(Heightmap { kind: kind, data: words[..] });
    }
    let buffer, e = wire::read_byte_array(r, wire::remaining(r)); if !wire::is_ok(e) { return ChunkData {}, e; }
    let n, e2 = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e2) { return ChunkData {}, e2; }
    var entities: BlockEntityInfo[>..] = [];
    for i in n {
        let xz, a = wire::read_i8(r); if !wire::is_ok(a) { return ChunkData {}, a; }
        let y, b = wire::read_i16(r); if !wire::is_ok(b) { return ChunkData {}, b; }
        let kind, c = wire::read_varint(r); if !wire::is_ok(c) { return ChunkData {}, c; }
        let tag, d = wire::read_nbt(r); if !wire::is_ok(d) { return ChunkData {}, d; }
        entities.push(BlockEntityInfo { packed_xz: xz, y: y, kind: kind, tag: tag[..] });
    }
    return ChunkData { heightmaps: heights[..], buffer: buffer, block_entities: entities[..] }, wire::Error.Ok;
}
fn write_chunk(data: ChunkData&, out: u8[>..]&) {
    wire::write_varint(out, data.heightmaps.len as! i32);
    for h in data.heightmaps { wire::write_varint(out, h.kind); wire::write_long_array(out, h.data); }
    wire::write_byte_array(out, data.buffer);
    wire::write_varint(out, data.block_entities.len as! i32);
    for e in data.block_entities { wire::write_i8(out, e.packed_xz); wire::write_i16(out, e.y); wire::write_varint(out, e.kind); wire::write_nbt(out, e.tag); }
}
// ChunkData.sections intentionally permits trailing bytes, like the source.
fn sections(data: ChunkData&, count: i64, block_bits: u8, biome_bits: u8) -> Section[>..], wire::Error {
    assert(count >= 0); var r = wire::reader(data.buffer); var out: Section[>..] = [];
    for i in count { let s, e = read_section(r, block_bits, biome_bits); if !wire::is_ok(e) { return [], e; } out.push(copy(s)); }
    return out, wire::Error.Ok;
}
fn from_sections(sections: Section[:]) -> u8[>..] {
    var out: u8[>..] = []; for s in sections { write_section(s, out); } return out;
}
fn read_layers(r: wire::Reader&) -> Layer[>..], wire::Error {
    let count, error = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(error) { return [], error; }
    var out: Layer[>..] = [];
    for i in count {
        let data, e = wire::read_byte_array(r, 2048); if !wire::is_ok(e) { return [], e; }
        if data.len != 2048 { return [], wire::Error.InvalidLayer { length: data.len }; }
        out.push(Layer { data: data });
    }
    return out, wire::Error.Ok;
}
fn write_layers(layers: Layer[:], out: u8[>..]&) {
    wire::write_varint(out, layers.len as! i32); for layer in layers { wire::write_byte_array(out, layer.data); }
}
fn read_light(r: wire::Reader&) -> LightData, wire::Error {
    let sky_mask, a = wire::read_byte_array(r, wire::remaining(r)); if !wire::is_ok(a) { return LightData {}, a; }
    let block_mask, b = wire::read_byte_array(r, wire::remaining(r)); if !wire::is_ok(b) { return LightData {}, b; }
    let empty_sky, c = wire::read_byte_array(r, wire::remaining(r)); if !wire::is_ok(c) { return LightData {}, c; }
    let empty_block, d = wire::read_byte_array(r, wire::remaining(r)); if !wire::is_ok(d) { return LightData {}, d; }
    let sky, e = read_layers(r); if !wire::is_ok(e) { return LightData {}, e; }
    let block_light, f = read_layers(r); if !wire::is_ok(f) { return LightData {}, f; }
    return LightData { sky_mask: sky_mask, block_mask: block_mask, empty_sky_mask: empty_sky, empty_block_mask: empty_block, sky: sky[..], block_light: block_light[..] }, wire::Error.Ok;
}
fn write_light(data: LightData&, out: u8[>..]&) {
    wire::write_bitset(out, data.sky_mask); wire::write_bitset(out, data.block_mask);
    wire::write_bitset(out, data.empty_sky_mask); wire::write_bitset(out, data.empty_block_mask);
    write_layers(data.sky, out); write_layers(data.block_light, out);
}
// Iterate variable-sized layers once; do not rescan for every mask bit.
struct LayerView { present: bool, data: const u8[:] }
fn expand_layers(mask: const u8[:], empty: const u8[:], source: Layer[:], count: i64, zero: const u8[:]) -> LayerView[>..] {
    var out: LayerView[>..] = []; var i: i64 = 0;
    for layer in source {
        while i < count && !wire::bit(mask, i) {
            let present = wire::bit(empty, i);
            out.push(LayerView { present: present, data: if present { zero[..] } else { zero[..0] } }); i += 1;
        }
        if i >= count { break; }
        out.push(LayerView { present: true, data: layer.data[..] }); i += 1;
    }
    while i < count {
        // An exhausted present-mask wins over an empty-mask, yielding None.
        let present = !wire::bit(mask, i) && wire::bit(empty, i);
        out.push(LayerView { present: present, data: if present { zero[..] } else { zero[..0] } }); i += 1;
    }
    return out;
}
fn layers(data: LightData&, count: i64) -> SectionLight[>..] {
    assert(count >= 0);
    let zero: u8[2048] = [0; 2048];
    let sky = expand_layers(data.sky_mask, data.empty_sky_mask, data.sky, count, zero);
    let block_light = expand_layers(data.block_mask, data.empty_block_mask, data.block_light, count, zero);
    var out: SectionLight[>..] = [];
    // Borrow during expansion: copy each present 2048-byte layer only once,
    // into the returned owner. Temporary tables hold just a flag and a slice.
    for s, i in sky {
        let b = block_light[i];
        out.push(SectionLight { has_sky: s.present, has_block: b.present, sky: s.data, block_light: b.data });
    }
    return out;
}
fn nibble(layer: const u8[:], index: i64) -> u8 {
    assert(index >= 0); if index >> 1 >= layer.len { return 0; }
    let b = layer[index >> 1]; return if index & 1 == 0 { b & 15 } else { b >> 4 };
}
