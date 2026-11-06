// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Adapted scalar position tests. Full notice: licenses/upstream-MIT.txt.
import std;
import core.position;
fn main() {
    let blocks: i32[5] = [-17, -16, -1, 0, 16];
    let sections: i32[5] = [-2, -1, -1, 0, 1];
    let locals: u16[5] = [15, 0, 15, 0, 0];
    for i in blocks.len {
        let p = position::block_pos(blocks[i], blocks[i], blocks[i]);
        let s = position::section(p);
        let c = position::chunk(p);
        assert(s.x == sections[i] && s.y == sections[i] && s.z == sections[i]);
        assert(c.x == sections[i] && c.z == sections[i]);
        let relative = position::section_relative(p);
        assert(relative == locals[i] * 0x111);
        let restored = position::relative_to_block(s, relative);
        assert(restored.x == p.x && restored.y == p.y && restored.z == p.z);
    }
    let r = position::region(position::chunk_pos(-1, -33));
    let lx, lz = position::region_local(position::chunk_pos(-1, -33));
    assert(r.x == -1 && r.z == -2 && lx == 31 && lz == 31);
    var packet: bool[4096] = [false; 4096];
    var storage: bool[4096] = [false; 4096];
    for y in 16 { for z in 16 { for x in 16 {
        let p = position::block_pos(x as i32, y as i32, z as i32);
        let wire_index = position::section_relative(p);
        let storage_index = position::section_index(p);
        assert(!packet[wire_index] && !storage[storage_index]);
        packet[wire_index] = true; storage[storage_index] = true;
        let restored = position::relative_to_block(position::SectionPos {}, wire_index);
        assert(restored.x == x && restored.y == y && restored.z == z);
    } } }
    for i in 4096 { assert(packet[i] && storage[i]); }
    assert(position::section_relative(position::block_pos(1, 2, 3)) == 0x132);
    assert(position::section_index(position::block_pos(1, 2, 3)) == 0x231);
    let p = position::block_pos(-33554432, -2048, 33554431);
    let restored = position::block_unpack(position::block_pack(p));
    assert(restored.x == p.x && restored.y == p.y && restored.z == p.z);
    let s = position::section_pos(-2097152, -524288, 2097151);
    let rs = position::section_unpack(position::section_pack(s));
    assert(rs.x == s.x && rs.y == s.y && rs.z == s.z);
    let c = position::chunk_pos(-2147483648, 2147483647);
    let rc = position::chunk_unpack(position::chunk_pack(c));
    assert(rc.x == c.x && rc.z == c.z);
    assert(position::offset(position::block_pos(2147483647, 0, 0), 1, 0, 0).x == -2147483648);
    print("Positions: negative floors, packet/storage layouts, signed packing and Java wrapping passed.");
}
