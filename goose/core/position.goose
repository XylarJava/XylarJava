// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Scalar position.rs port. Full notice: ../licenses/upstream-MIT.txt.
// Box iterators in position/iter.rs are a separate, still pending unit.
namespace position;
struct BlockPos { x: i32 = 0, y: i32 = 0, z: i32 = 0 }
struct SectionPos { x: i32 = 0, y: i32 = 0, z: i32 = 0 }
struct ChunkPos { x: i32 = 0, z: i32 = 0 }
fn block_pos(x: i32, y: i32, z: i32) -> BlockPos { BlockPos { x, y, z } }
fn section_pos(x: i32, y: i32, z: i32) -> SectionPos { SectionPos { x, y, z } }
fn chunk_pos(x: i32, z: i32) -> ChunkPos { ChunkPos { x, z } }
fn block_pack(p: BlockPos) -> i64 {
    return ((((p.x as! u64) & 0x3ffffff) << 38) |
            (((p.z as! u64) & 0x3ffffff) << 12) | ((p.y as! u64) & 0xfff)) as! i64;
}
fn block_unpack(value: i64) -> BlockPos {
    let bits = value as! u64;
    return BlockPos { x: (value >> 38) as i32,
                      y: (((bits << 52) as! i64) >> 52) as i32,
                      z: (((bits << 26) as! i64) >> 38) as i32 };
}
fn offset(p: BlockPos, dx: i32, dy: i32, dz: i32) -> BlockPos {
    return BlockPos { x: ((p.x as! u32) + (dx as! u32)) as! i32,
                      y: ((p.y as! u32) + (dy as! u32)) as! i32,
                      z: ((p.z as! u32) + (dz as! u32)) as! i32 };
}
fn section(p: BlockPos) -> SectionPos { SectionPos { p.x >> 4, p.y >> 4, p.z >> 4 } }
fn chunk(p: BlockPos) -> ChunkPos { ChunkPos { p.x >> 4, p.z >> 4 } }
fn section_relative(p: BlockPos) -> u16 {
    return (((p.x & 15) << 8) | ((p.z & 15) << 4) | (p.y & 15)) as u16;
}
fn section_index(p: BlockPos) -> i64 {
    return ((p.y & 15) << 8) | ((p.z & 15) << 4) | (p.x & 15);
}
fn section_pack(p: SectionPos) -> i64 {
    return ((((p.x as! u64) & 0x3fffff) << 42) |
            (((p.z as! u64) & 0x3fffff) << 20) | ((p.y as! u64) & 0xfffff)) as! i64;
}
fn section_unpack(value: i64) -> SectionPos {
    let bits = value as! u64;
    return SectionPos { x: (value >> 42) as i32,
                        y: (((bits << 44) as! i64) >> 44) as i32,
                        z: (((bits << 22) as! i64) >> 42) as i32 };
}
fn relative_to_block(p: SectionPos, relative: u16) -> BlockPos {
    return BlockPos {
        x: (((p.x as! u32) << 4) + ((relative >> 8) & 15)) as! i32,
        y: (((p.y as! u32) << 4) + (relative & 15)) as! i32,
        z: (((p.z as! u32) << 4) + ((relative >> 4) & 15)) as! i32,
    };
}
fn chunk_pack(p: ChunkPos) -> i64 {
    return ((p.x as! u32) | (((p.z as! u32) as u64) << 32)) as! i64;
}
fn chunk_unpack(value: i64) -> ChunkPos { ChunkPos { value as! i32, (value >> 32) as i32 } }
fn region(p: ChunkPos) -> ChunkPos { ChunkPos { p.x >> 5, p.z >> 5 } }
fn region_local(p: ChunkPos) -> u8, u8 { return (p.x & 31) as u8, (p.z & 31) as u8; }
