import std;
import os;
import core.position;
fn main() {
    var input: u8[>..] = [];
    read_stdin(input);
    for line in split(input, '\n') {
        if line.len == 0 { continue; }
        let parts = split(line, ' ');
        fn number(i: i64) -> i64 { let v, ok = parse_int(parts[i]); assert(ok); return v; }
        let p = position::block_pos(number(0) as i32, number(1) as i32, number(2) as i32);
        let packed = position::block_pack(p);
        let unp = position::block_unpack(packed);
        let off = position::offset(p, number(3) as i32, number(4) as i32, number(5) as i32);
        let sec = position::section(p);
        let ch = position::chunk(p);
        let rel = position::section_relative(p);
        let sp = position::section_pack(position::section_pos(p.x, p.y, p.z));
        let su = position::section_unpack(sp);
        let rb = position::relative_to_block(position::section_pos(p.x, p.y, p.z), number(7) as u16);
        let cp = position::chunk_pack(position::chunk_pos(p.x, p.z));
        let cu = position::chunk_unpack(cp);
        let reg = position::region(position::chunk_pos(p.x, p.z));
        let lx, lz = position::region_local(position::chunk_pos(p.x, p.z));
        let braw = position::block_unpack(number(6));
        let sraw = position::section_unpack(number(6));
        let craw = position::chunk_unpack(number(6));
        print(packed, " ", unp.x, " ", unp.y, " ", unp.z, " ", off.x, " ", off.y, " ", off.z,
              " ", sec.x, " ", sec.y, " ", sec.z, " ", ch.x, " ", ch.z, " ", rel, " ", position::section_index(p),
              " ", sp, " ", su.x, " ", su.y, " ", su.z, " ", rb.x, " ", rb.y, " ", rb.z,
              " ", cp, " ", cu.x, " ", cu.z, " ", reg.x, " ", reg.z, " ", lx, " ", lz,
              " ", braw.x, " ", braw.y, " ", braw.z, " ", sraw.x, " ", sraw.y, " ", sraw.z,
              " ", craw.x, " ", craw.z);
    }
}
