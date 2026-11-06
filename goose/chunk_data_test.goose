// Independent wire vectors: section counts, light-mask priority and nibble order.
import std;
import net.play;
fn main() {
    let raw: u8[] = [0, 8, 1, 0, 0, 0, 0, 1, 0, 2, 1, 63, 255, 196, 2, 0];
    var r = wire::reader(raw);
    let chunk, e = chunk_wire::read_chunk(r); assert(wire::is_ok(e) && r.pos == raw.len);
    assert(chunk.block_entities.len == 1);
    for entity in chunk.block_entities { assert(entity.packed_xz == 63 && entity.y == -60 && entity.kind == 2 && entity.tag.len == 1); }
    let sections, se = chunk_wire::sections(chunk, 1, 16, 7); assert(wire::is_ok(se));
    for s in sections { assert(s.non_empty == 256 && s.fluids == 0); assert(chunk_wire::get(s.states,4095) == 1 && chunk_wire::get(s.biomes,63) == 2); }
    let encoded = chunk_wire::from_sections(sections); assert(encoded == raw[2..10]);
    let missing, me = chunk_wire::sections(chunk, 2, 16, 7); assert(!wire::is_ok(me));
    var rewritten: u8[>..] = []; chunk_wire::write_chunk(chunk,rewritten); assert(rewritten == raw);
    let bytes: u8[2048] = [0xf0; 2048];
    let lights: chunk_wire::Layer[] = [chunk_wire::Layer { data: bytes[..] }];
    let light = chunk_wire::LightData { sky_mask: [12], empty_sky_mask: [14], empty_block_mask: [2], sky: lights[..] };
    let expanded = chunk_wire::layers(light, 5);
    for l, i in expanded {
        if i == 1 { assert(l.has_sky && l.has_block && l.sky.len == 2048 && l.block_light.len == 2048); assert(chunk_wire::nibble(l.sky,4095)==0); }
        else if i == 2 { assert(l.has_sky && !l.has_block); assert(chunk_wire::nibble(l.sky,0)==0 && chunk_wire::nibble(l.sky,1)==15 && chunk_wire::nibble(l.sky,4095)==15 && chunk_wire::nibble(l.sky,4096)==0); }
        else { assert(!l.has_sky && !l.has_block); }
    }
    let bad: u8[2] = [1, 0]; var bad_reader = wire::reader(bad);
    let ignored, le = chunk_wire::read_layers(bad_reader); let message = wire::error_message(le);
    assert(message == "DataLayer should be 2048 bytes not: 0" && bad_reader.pos == 2);
    let section=position::section_pack(position::SectionPos {x:-2,y:-4,z:3});
    let pos,state=play263::section_change(section,(35722 as i64)*4096+0x1f3);
    assert(pos.x== -31 && pos.y== -61 && pos.z==63 && state==35722);
    let negative,negative_state=play263::section_change(section,-1);
    assert(negative.x== -17 && negative.y== -49 && negative.z==63 && negative_state== -1);
    let unload:play263::Clientbound..=play263::Clientbound.ForgetLevelChunk {packed_pos:position::chunk_pack(position::ChunkPos {x:-1,z:2})};
    let encoded_unload=play263::encode_clientbound(unload);
    assert(encoded_unload.len==9 && encoded_unload[0]==38 && encoded_unload[4]==2 && encoded_unload[8]==255);
    print("Chunk columns, section extraction, light mask precedence and nibbles passed.");
}
