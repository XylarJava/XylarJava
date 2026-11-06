// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Rust-to-Goose adaptation. Full notice: ../licenses/upstream-MIT.txt.
// Reference: paletted.rs in ../upstream.lock.json.
import std;
import dictionary;
import core.storage;
namespace paletted;

let BLOCKS: u8 = 0;
let BIOMES: u8 = 1;
let SINGLE: u8 = 0;
let LIST: u8 = 1;
let GLOBAL: u8 = 2;
struct Strategy { kind: u8 = 0, global_bits: u8 = 16 }
fn blocks(global_bits: u8) -> Strategy { Strategy { kind: BLOCKS, global_bits: global_bits } }
fn biomes(global_bits: u8) -> Strategy { Strategy { kind: BIOMES, global_bits: global_bits } }
fn entries(strategy: Strategy) -> i64 { if strategy.kind == BLOCKS { 4096 } else { 64 } }
fn config(strategy: Strategy, bits: u8) -> u8, u8 {
    if bits == 0 { return SINGLE, 0; }
    if strategy.kind == BLOCKS && bits <= 8 { return LIST, max(4, bits); }
    if strategy.kind == BIOMES && bits <= 3 { return LIST, bits; }
    return GLOBAL, strategy.global_bits;
}
fn wire_has_palette(strategy: Strategy, bits: u8) -> bool {
    let mode, width = config(strategy, bits);
    return mode != GLOBAL;
}
fn ceil_log2(n: i64) -> u8 {
    assert(n >= 0);
    if n <= 1 { return 0; }
    var v = n - 1;
    var bits: u8 = 0;
    while v > 0 { v >>= 1; bits += 1; }
    return bits;
}
fn storage_len(bits: u8, count: i64) -> i64 {
    assert(count >= 0);
    if bits == 0 { return 0; }
    assert(bits <= 32);
    let per = 64 / (bits as i64);
    return count / per + (if count % per != 0 { 1 } else { 0 });
}
fn read_word(words: u64[:], bits: u8, index: i64) -> u32 {
    let word, shift = packed::locate(bits, index);
    return ((words[word] >> shift) & packed::mask(bits)) as u32;
}
fn write_word(words: u64[:], bits: u8, index: i64, value: u32) {
    let word, shift = packed::locate(bits, index);
    let mask = packed::mask(bits);
    words[word] = (words[word] & ~(mask << shift)) | ((value as u64) << shift);
}
struct Ids { palette: u32[] = [], ids: u32[] = [] }
let DENSE_VALUES: i64 = 1 << 16;
// Every Goose worker owns its globals; clear only touched dense entries.
var dense_ids: u32[DENSE_VALUES];
fn find_id(values: u32[:], value: u32) -> i64 {
    for i in values.len { if values[i] == value { return i; } }
    return -1;
}
fn palette_ids(values: u32[:]) -> Ids {
    var palette: u32[>..] = [];
    var ids: u32[>..] = [];
    var cursor: i64 = 0;
    var have_last = false;
    var last_value: u32 = 0;
    var last_id: u32 = 0;
    while cursor < values.len {
        let value = values[cursor];
        var id: u32 = 0;
        if have_last && value == last_value { id = last_id; }
        else {
            if palette.len > 16 { break; }
            let existing = find_id(palette, value);
            if existing >= 0 { id = existing as u32; }
            else { palette.push(value); id = (palette.len - 1) as u32; }
        }
        have_last = true; last_value = value; last_id = id;
        ids.push(id); cursor += 1;
    }
    if cursor == values.len { return Ids { palette: palette[..], ids: ids[..] }; }
    var dense = true;
    for value in palette { if value >= DENSE_VALUES { dense = false; } }
    for value in values[cursor..] { if value >= DENSE_VALUES { dense = false; } }
    if dense {
        for i in palette.len { dense_ids[palette[i]] = (i + 1) as u32; }
        for value in values[cursor..] {
            var id = dense_ids[value];
            if id == 0 { palette.push(value); id = palette.len as u32; dense_ids[value] = id; }
            ids.push(id - 1);
        }
        for value in palette { dense_ids[value] = 0; }
    } else {
        var lookup = dictionary<u32, u32> {};
        for i in palette.len { ::insert(lookup, palette[i], i as u32); }
        for value in values[cursor..] {
            let found = ::dict_probe(lookup, value);
            var id: u32 = 0;
            if found >= 0 { id = lookup.slots[found].val; }
            else { id = palette.len as u32; palette.push(value); ::insert(lookup, value, id); }
            ids.push(id);
        }
    }
    return Ids { palette: palette[..], ids: ids[..] };
}
fn pack_words(bits: u8, count: i64, ids: u32[:]) -> u64[>..] {
    var words: u64[>..] = [];
    for i in storage_len(bits, count) { words.push(0); }
    for i in ids.len { write_word(words, bits, i, ids[i]); }
    return words;
}
enum PaletteError {
    Ok,
    BadPalette { len: i64 },
    BadStorage { expected: i64, actual: i64 },
    BadIndex { index: u32, palette: i64 },
    BadBits { declared: u8, expected: u8 },
}
fn is_ok(error: PaletteError) -> bool { match error { Ok => true, _ => false } }
// One growable owner: raw words first, followed by list palette ids. This
// avoids independent resizable fields (not supported by Goose) and a fixed
// 256-entry reservation in every single-value container. Palette values use
// u64 slots in memory; wire and saved palettes remain u32.
struct Container {
    strategy: Strategy = Strategy {}, bits: u8 = 0, mode: u8 = 0,
    single: u32 = 0, word_count: i64 = 0, palette_count: i64 = 0,
    data: u64[>..] = [],
}
struct Packed {
    bits: u8 = 0, has_data: bool = false,
    palette: u32[] = [], data: u64[] = [],
}
struct Wire { bits: u8 = 0, palette: u32[] = [], words: u64[] = [] }
fn words(c: Container&) -> u64[:] { c.data[0..c.word_count] }
fn palette_value(c: Container&, index: i64) -> u32 {
    assert(index >= 0 && index < c.palette_count);
    return c.data[c.word_count + index] as u32;
}
fn palette_values(c: Container&) -> u32[>..] {
    var values: u32[>..] = [];
    for i in c.palette_count { values.push(palette_value(c, i)); }
    return values;
}
fn find_palette(c: Container&, value: u32) -> i64 {
    for i in c.palette_count { if palette_value(c, i) == value { return i; } }
    return -1;
}
fn append_palette(c: Container&, value: u32) {
    c.data.push(value);
    c.palette_count += 1;
}
fn new_container(strategy: Strategy, value: u32) -> Container {
    Container { strategy: strategy, single: value }
}
fn configured(strategy: Strategy, requested_bits: u8, single: u32 = 0) -> Container {
    let mode, bits = config(strategy, requested_bits);
    var c = Container { strategy: strategy, bits: bits, mode: mode, single: single };
    c.word_count = storage_len(bits, entries(strategy));
    for i in c.word_count { c.data.push(0); }
    return c;
}
fn palette_len(c: Container&) -> i64 {
    if c.mode == SINGLE { return 1; }
    if c.mode == LIST { return c.palette_count; }
    return (1 as i64) << min(c.bits, 31);
}
fn is_global(c: Container&) -> bool { c.mode == GLOBAL }
fn raw(c: Container&, index: i64) -> u32 { read_word(words(c), c.bits, index) }
fn get(c: Container&, index: i64) -> u32 {
    assert(index >= 0 && index < entries(c.strategy));
    if c.mode == SINGLE { return c.single; }
    let id = raw(c, index);
    if c.mode == LIST { return palette_value(c, id); }
    return id;
}
fn get_run(c: Container&, start: i64, output: u32[:]) {
    assert(start >= 0 && start <= entries(c.strategy) && output.len <= entries(c.strategy) - start);
    if c.mode == SINGLE { ::fill(output, c.single); return; }
    if c.mode == LIST {
        for i in output.len { output[i] = palette_value(c, raw(c, start + i)); }
    } else {
        for i in output.len { output[i] = raw(c, start + i); }
    }
}
fn all_equal(c: Container&, value: u32) -> bool {
    if c.mode == SINGLE { return c.single == value; }
    var id: u64 = value;
    if c.mode == LIST {
        let found = find_palette(c, value);
        if found < 0 { return false; }
        id = found as u64;
    }
    let per = 64 / (c.bits as i64);
    var pattern: u64 = 0;
    for field in per { pattern |= id << (field * c.bits); }
    let count = entries(c.strategy);
    let full = count / per;
    for i in full { if c.data[i] != pattern { return false; } }
    let tail = count - full * per;
    if tail == 0 { return true; }
    let used = ((1 as u64) << (tail * c.bits)) - 1;
    return (c.data[full] & used) == (pattern & used);
}
fn is_uniform(c: Container&, value: u32) -> bool {
    if c.mode == SINGLE { return c.single == value; }
    if c.mode == LIST {
        if find_palette(c, value) < 0 { return false; }
        if c.palette_count == 1 { return true; }
    }
    return all_equal(c, value);
}
fn uniform(c: Container&) -> u32, bool {
    if c.mode == SINGLE { return c.single, true; }
    if c.mode == LIST && c.palette_count == 1 { return palette_value(c, 0), true; }
    let first = get(c, 0);
    return first, all_equal(c, first);
}
fn equal_u32(a: u32[:], b: u32[:]) -> bool {
    if a.len != b.len { return false; }
    for i in a.len { if a[i] != b[i] { return false; } }
    return true;
}
fn equal_u64(a: u64[:], b: u64[:]) -> bool {
    if a.len != b.len { return false; }
    for i in a.len { if a[i] != b[i] { return false; } }
    return true;
}
fn same_storage(a: Container&, b: Container&) -> bool {
    if a.bits != b.bits || a.mode != b.mode || !equal_u64(words(a), words(b)) { return false; }
    if a.mode == SINGLE { return a.single == b.single; }
    if a.mode == LIST {
        if a.palette_count != b.palette_count { return false; }
        for i in a.palette_count { if palette_value(a, i) != palette_value(b, i) { return false; } }
    }
    return true;
}
fn equal(a: Container&, b: Container&) -> bool {
    if entries(a.strategy) != entries(b.strategy) { return false; }
    if same_storage(a, b) { return true; }
    for i in entries(a.strategy) { if get(a, i) != get(b, i) { return false; } }
    return true;
}
// Adds an id to a freshly configured palette, without triggering resize.
fn id_in(c: Container&, value: u32) -> u32 {
    if c.mode == SINGLE { return 0; }
    if c.mode == GLOBAL { return value; }
    let old = find_palette(c, value);
    if old >= 0 { return old as u32; }
    append_palette(c, value);
    return (c.palette_count - 1) as u32;
}
fn resize_container(c: Container&, bits: u8, last: u32) -> u32 {
    var fresh = configured(c.strategy, bits, last);
    let count = entries(c.strategy);
    if c.mode == SINGLE {
        let id = id_in(fresh, c.single);
        if fresh.bits > 0 && id != 0 {
            for i in count { write_word(words(fresh), fresh.bits, i, id); }
        }
    } else {
        var last_old: u32 = 4294967295;
        var new_id: u32 = 0;
        for i in count {
            let old = raw(c, i);
            if old != last_old {
                last_old = old;
                let value = if c.mode == LIST { palette_value(c, old) } else { old };
                new_id = id_in(fresh, value);
            }
            if fresh.bits > 0 { write_word(words(fresh), fresh.bits, i, new_id); }
        }
    }
    let id = id_in(fresh, last);
    c = copy(fresh);
    return id;
}
fn id_for(c: Container&, value: u32) -> u32 {
    if c.mode == SINGLE {
        if c.single == value { return 0; }
        return resize_container(c, 1, value);
    }
    if c.mode == GLOBAL { return value; }
    let old = find_palette(c, value);
    if old >= 0 { return old as u32; }
    if c.palette_count < (1 as i64) << c.bits {
        append_palette(c, value);
        return (c.palette_count - 1) as u32;
    }
    return resize_container(c, c.bits + 1, value);
}
fn set(c: Container&, index: i64, value: u32) -> u32 {
    assert(index >= 0 && index < entries(c.strategy));
    let id = id_for(c, value);
    if c.mode == SINGLE { return c.single; }
    let old = raw(c, index);
    write_word(words(c), c.bits, index, id);
    return if c.mode == LIST { palette_value(c, old) } else { old };
}
fn unpack(c: Container&, output: u32[:]) {
    assert(output.len == entries(c.strategy));
    if c.mode == SINGLE { ::fill(output, c.single); return; }
    let per = 64 / (c.bits as i64);
    let mask = packed::mask(c.bits);
    var index: i64 = 0;
    for word in words(c) {
        for field in min(per, output.len - index) {
            let id = ((word >> (field * c.bits)) & mask) as u32;
            output[index] = if c.mode == LIST { palette_value(c, id) } else { id };
            index += 1;
        }
    }
}
// Returns whether the caller's ids were filled; the palette itself remains in c.
fn unpack_ids(c: Container&, ids: u16[:]) -> bool {
    if c.mode != LIST { return false; }
    assert(ids.len == entries(c.strategy));
    let per = 64 / (c.bits as i64);
    let mask = packed::mask(c.bits);
    var index: i64 = 0;
    for word in words(c) {
        for field in min(per, ids.len - index) {
            ids[index] = ((word >> (field * c.bits)) & mask) as u16;
            index += 1;
        }
    }
    return true;
}
fn build(strategy: Strategy, palette: u32[:], ids: u32[:]) -> Container {
    assert(palette.len > 0);
    let requested = ceil_log2(palette.len);
    var c = configured(strategy, requested, palette[0]);
    if c.mode == SINGLE { return c; }
    if c.mode == LIST { for value in palette { append_palette(c, value); } }
    for i in ids.len {
        write_word(words(c), c.bits, i, if c.mode == GLOBAL { palette[ids[i]] } else { ids[i] });
    }
    return c;
}
fn from_values(strategy: Strategy, values: u32[:]) -> Container {
    assert(values.len == entries(strategy));
    let ids = palette_ids(values);
    return build(strategy, ids.palette, ids.ids);
}
fn check_indices(c: Container&) -> PaletteError {
    if c.mode != LIST || c.palette_count == (1 as i64) << c.bits { return PaletteError.Ok; }
    let per = 64 / (c.bits as i64);
    let mask = packed::mask(c.bits);
    var remaining = entries(c.strategy);
    for word in words(c) {
        let count = min(remaining, per);
        for field in count {
            let id = ((word >> (field * c.bits)) & mask) as u32;
            if id >= c.palette_count { return PaletteError.BadIndex { index: id, palette: c.palette_count }; }
        }
        remaining -= count;
    }
    return PaletteError.Ok;
}
fn from_wire(strategy: Strategy, declared: u8, palette: u32[:], words: u64[:]) -> Container, PaletteError {
    let mode, bits = config(strategy, declared);
    let expected = storage_len(bits, entries(strategy));
    if words.len != expected {
        return new_container(strategy, 0), PaletteError.BadStorage { expected: expected, actual: words.len };
    }
    if mode == SINGLE {
        if palette.len != 1 { return new_container(strategy, 0), PaletteError.BadPalette { len: palette.len }; }
        return new_container(strategy, palette[0]), PaletteError.Ok;
    }
    if mode == LIST && (palette.len == 0 || palette.len > (1 as i64) << bits) {
        return new_container(strategy, 0), PaletteError.BadPalette { len: palette.len };
    }
    var c = Container { strategy: strategy, mode: mode, bits: bits };
    c.word_count = words.len;
    for word in words { c.data.push(word); }
    if mode == LIST { for value in palette { append_palette(c, value); } }
    let error = check_indices(c);
    if !is_ok(error) { return new_container(strategy, 0), error; }
    return c, PaletteError.Ok;
}
fn to_wire(c: Container&) -> Wire {
    var palette: u32[>..] = [];
    if c.mode == SINGLE { palette.push(c.single); }
    else if c.mode == LIST { for i in c.palette_count { palette.push(palette_value(c, i)); } }
    return Wire { bits: c.bits, palette: palette[..], words: words(c) };
}
fn pack(c: Container&) -> Packed {
    var values: u32[>..] = [];
    for i in entries(c.strategy) { values.push(0); }
    unpack(c, values);
    let ids = palette_ids(values);
    let mode, width = config(c.strategy, ceil_log2(ids.palette.len));
    let bits = if mode == GLOBAL { ceil_log2(ids.palette.len) } else { width };
    var data: u64[>..] = [];
    if bits != 0 { data = pack_words(bits, entries(c.strategy), ids.ids); }
    return Packed { bits: bits, has_data: bits != 0, palette: ids.palette[..], data: data[..] };
}
fn from_packed(strategy: Strategy, palette: u32[:], has_data: bool, data: u64[:]) -> Container, PaletteError {
    if palette.len == 0 { return new_container(strategy, 0), PaletteError.BadPalette { len: 0 }; }
    let mode, width = config(strategy, ceil_log2(palette.len));
    if mode == SINGLE { return new_container(strategy, palette[0]), PaletteError.Ok; }
    let bits = if mode == GLOBAL { ceil_log2(palette.len) } else { width };
    let expected = storage_len(bits, entries(strategy));
    if !has_data || data.len != expected {
        return new_container(strategy, 0), PaletteError.BadStorage { expected: expected, actual: if has_data { data.len } else { 0 } };
    }
    if mode == LIST { return from_wire(strategy, bits, palette, data); }
    var ids: u32[>..] = [];
    for i in entries(strategy) {
        let index = read_word(data, bits, i);
        if index >= palette.len { return new_container(strategy, 0), PaletteError.BadIndex { index: index, palette: palette.len }; }
        ids.push(index);
    }
    return build(strategy, palette, ids), PaletteError.Ok;
}
fn maybe_has<Predicate>(c: Container&) -> bool {
    if c.mode == SINGLE { return Predicate(c.single); }
    if c.mode == LIST { for i in c.palette_count { if Predicate(palette_value(c, i)) { return true; } } }
    else { for value in palette_len(c) { if Predicate(value as u32) { return true; } } }
    return false;
}
fn has<Predicate>(c: Container&) -> bool {
    if c.mode == SINGLE { return Predicate(c.single); }
    if c.mode == GLOBAL {
        for i in entries(c.strategy) { if Predicate(raw(c, i)) { return true; } }
        return false;
    }
    var matching: bool[>..] = [];
    var count: i64 = 0;
    for i in c.palette_count {
        let hit = Predicate(palette_value(c, i));
        matching.push(hit);
        if hit { count += 1; }
    }
    if count == 0 || count == c.palette_count { return count != 0; }
    let per = 64 / (c.bits as i64);
    let size = entries(c.strategy);
    let full = size / per;
    if count <= 4 {
        var lo: u64 = 0;
        for field in per { lo |= (1 as u64) << (field * c.bits); }
        let hi = lo << (c.bits - 1);
        for id in matching.len {
            if !matching[id] { continue; }
            let pattern = lo * (id as u64);
            for i in full {
                let value = c.data[i] ^ pattern;
                if ((value - lo) & ~value & hi) != 0 { return true; }
            }
        }
        for i in full * per..size { if matching[raw(c, i)] { return true; } }
        return false;
    }
    for i in size { if matching[raw(c, i)] { return true; } }
    return false;
}
fn word_bytes(c: Container&) -> i64 {
    c.data.len * 8
}
