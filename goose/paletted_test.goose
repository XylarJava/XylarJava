// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Adapted behavioral tests for the translated container. See licenses/upstream-MIT.txt.
import std;
import core.paletted;

fn test_growth() {
    var c = paletted::new_container(paletted::blocks(16), 0);
    assert(c.bits == 0 && paletted::palette_len(c) == 1);
    for n in 1..301 {
        assert(paletted::set(c, n, n as u32) == 0);
        let count = n + 1;
        let expected = if count <= 16 { 4 } else if count <= 32 { 5 }
                       else if count <= 64 { 6 } else if count <= 128 { 7 }
                       else if count <= 256 { 8 } else { 16 };
        assert(c.bits == expected);
    }
    assert(paletted::is_global(c));
    for n in 1..301 { assert(paletted::get(c, n) == n); }
    let wire = paletted::to_wire(c);
    var wire_copy, wire_error = paletted::from_wire(c.strategy, wire.bits, wire.palette, wire.words);
    assert(paletted::is_ok(wire_error) && paletted::same_storage(c, wire_copy));
    let saved = paletted::pack(c);
    var restored, error = paletted::from_packed(c.strategy, saved.palette, saved.has_data, saved.data);
    assert(paletted::is_ok(error) && paletted::equal(c, restored));
    var b = paletted::new_container(paletted::biomes(7), 3);
    for n in 1..10 {
        paletted::set(b, n, (n + 3) as u32);
        let expected = if n == 1 { 1 } else if n <= 3 { 2 } else if n <= 7 { 3 } else { 7 };
        assert(b.bits == expected);
    }
    assert(paletted::is_global(b));
}
fn test_predicates_and_uniform() {
    var c = paletted::new_container(paletted::blocks(16), 0);
    assert(paletted::has(c) { it == 0 });
    paletted::set(c, 4095, 3);
    assert(paletted::has(c) { it == 3 });
    paletted::set(c, 4095, 0);
    assert(paletted::maybe_has(c) { it == 3 });
    assert(!paletted::has(c) { it == 3 });
    let value, found = paletted::uniform(c);
    assert(found && value == 0 && paletted::palette_len(c) == 2);
    for i in 4096 { paletted::set(c, i, 3); }
    assert(paletted::is_uniform(c, 3) && !paletted::is_uniform(c, 0));
    let compact = paletted::pack(c);
    assert(compact.bits == 0 && !compact.has_data && compact.palette.len == 1 && compact.palette[0] == 3);
    for n in 1..301 { paletted::set(c, n, n as u32); }
    paletted::set(c, 300, 3);
    assert(paletted::maybe_has(c) { it == 300 });
    assert(!paletted::has(c) { it == 300 });
}
fn test_values() {
    for distinct in [1, 5, 17, 129, 256, 257, 600] {
        var values: u32[>..] = [];
        for i in 4096 { values.push((i % distinct) as u32); }
        var c = paletted::from_values(paletted::blocks(16), values);
        var out: u32[4096] = [0; 4096];
        paletted::unpack(c, out);
        for i in out.len { assert(out[i] == values[i] && paletted::get(c, i) == values[i]); }
        var ids: u16[4096] = [65535; 4096];
        let has_ids = paletted::unpack_ids(c, ids);
        assert(has_ids == (c.mode == paletted::LIST));
        if has_ids { for i in ids.len { assert(paletted::palette_value(c, ids[i]) == values[i]); } }
        else { for id in ids { assert(id == 65535); } }
        for start in [0, 1, 15, 16, 4080] {
            var run: u32[16] = [0; 16];
            paletted::get_run(c, start, run);
            for i in run.len { assert(run[i] == values[start + i]); }
        }
        for wanted in 0..20 {
            let hit = paletted::has(c) { it == wanted };
            assert(hit == (wanted < distinct));
        }
        let wire = paletted::to_wire(c);
        var loaded, error = paletted::from_wire(c.strategy, wire.bits, wire.palette, wire.words);
        assert(paletted::is_ok(error) && paletted::same_storage(c, loaded));
        let saved = paletted::pack(c);
        var restored, packed_error = paletted::from_packed(c.strategy, saved.palette, saved.has_data, saved.data);
        assert(paletted::is_ok(packed_error) && paletted::equal(c, restored));
        let again = paletted::pack(restored);
        assert(saved.bits == again.bits && saved.has_data == again.has_data);
        assert(paletted::equal_u32(saved.palette, again.palette) && paletted::equal_u64(saved.data, again.data));
        var copy_c = copy(c);
        assert(paletted::same_storage(c, copy_c));
    }
    // Exercise the sparse hash path rather than the reusable dense table.
    for pass in 2 {
        var values: u32[>..] = [];
        for i in 4096 { values.push((70000 + i % 40) as u32); }
        let ids = paletted::palette_ids(values);
        assert(ids.palette.len == 40);
        for i in values.len { assert(ids.palette[ids.ids[i]] == values[i]); }
    }
    for slot in paletted::dense_ids { assert(slot == 0); }
}
fn test_validation() {
    let strategy = paletted::blocks(16);
    let palette: u32[2] = [1, 2];
    var short: u64[255] = [0; 255];
    var invalid, short_error = paletted::from_wire(strategy, 4, palette, short);
    match short_error {
        BadStorage e => { assert(e.expected == 256 && e.actual == 255); },
        _ => { abort("Wrong validation order"); },
    }
    var words: u64[256] = [0; 256];
    words[0] = 3;
    var invalid_index, index_error = paletted::from_wire(strategy, 4, palette, words);
    match index_error {
        BadIndex e => { assert(e.index == 3 && e.palette == 2); },
        _ => { abort("Missing palette index validation"); },
    }
    let empty: u64[0] = [];
    var invalid_single, single_error = paletted::from_wire(strategy, 0, palette, empty);
    assert(!paletted::is_ok(single_error));
    let only: u32[1] = [9];
    var single, error = paletted::from_packed(strategy, only, true, words);
    assert(paletted::is_ok(error) && paletted::is_uniform(single, 9));
    var absent, absent_error = paletted::from_packed(strategy, palette, false, empty);
    assert(!paletted::is_ok(absent_error));
}
fn main() {
    test_growth();
    test_predicates_and_uniform();
    test_values();
    test_validation();
    print("Paletted containers: growth, wire/save round trips, predicates, dense/sparse ids and validation passed.");
}
