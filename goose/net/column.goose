// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Ownership adapter binding translated ChunkData.sections and LightData.layers
// to a configured vertical layout. Not the renderer's complete World store.
import std;
import net.registry;
import net.chunk;
namespace column263;
struct Column { x:i32=0,z:i32=0,layout:registry263::Layout=registry263::Layout {},
    sections:chunk_wire::Section[]=[],light:chunk_wire::SectionLight[]=[],entities:chunk_wire::BlockEntityInfo[]=[] }
fn bind(x:i32,z:i32,chunk:chunk_wire::ChunkData&,light:chunk_wire::LightData&,layout:registry263::Layout,block_bits:u8,biome_bits:u8) -> Column,wire::Error {
    assert(registry263::valid_shape(layout.min_y,layout.height));
    let sections,e=chunk_wire::sections(chunk,registry263::sections(layout),block_bits,biome_bits);
    if !wire::is_ok(e) {return Column {},e;}
    let layers=chunk_wire::layers(light,registry263::sections(layout)+2);
    return Column {x:x,z:z,layout:layout,sections:sections[..],light:layers[..],entities:chunk.block_entities[..]},wire::Error.Ok;
}
// Local X/Z and absolute Y. Keep sparse palettes resident; do not inflate every
// section to 4096 integers. Variable-sized sections currently iterate by Y.
fn block_at(column:Column&,x:i32,y:i32,z:i32) -> i32,bool {
    let relative=(y as i64)-column.layout.min_y;
    if x<0 || x>=16 || z<0 || z>=16 || relative<0 || relative>=column.layout.height {return 0,false;}
    for section,i in column.sections {
        if i==relative/16 {return chunk_wire::get(section.states,(relative%16)*256+z*16+x),true;}
    }
    return 0,false;
}
fn biome_at(column:Column&,x:i32,y:i32,z:i32) -> i32,bool {
    let relative=(y as i64)-column.layout.min_y;
    if x<0 || x>=16 || z<0 || z>=16 || relative<0 || relative>=column.layout.height {return 0,false;}
    for section,i in column.sections {
        if i==relative/16 {return chunk_wire::get(section.biomes,((relative%16)/4)*16+(z/4)*4+x/4),true;}
    }
    return 0,false;
}
