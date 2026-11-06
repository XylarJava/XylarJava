// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// registry.rs::Dynamic.dimension / DimensionType layout subset.
// Registry names, dialog tags, default_clock and built-in reports remain pending.
import std;
import core.jmath;
import net.configuration;
namespace registry263;
struct Layout { min_y: i32 = -64, height: i32 = 384, logical_height: i32 = 384,
    has_skylight: bool = true, has_ceiling: bool = false, ambient_light: f32 = 0.0 }
fn valid_shape(min_y: i32, height: i32) -> bool {
    return min_y % 16 == 0 && height % 16 == 0 && height >= 16 && min_y >= -2032 && (min_y as i64) + height <= 2032;
}
fn vanilla(name: const u8[:]) -> Layout {
    if name == "minecraft:the_nether" { return Layout { min_y: 0, height: 256, logical_height: 128, has_skylight: false, has_ceiling: true, ambient_light: 0.1 }; }
    if name == "minecraft:the_end" { return Layout { min_y: 0, height: 256, logical_height: 256, has_skylight: true, has_ceiling: false, ambient_light: 0.25 }; }
    return Layout { has_ceiling: name == "minecraft:overworld_caves" };
}
fn sections(layout: Layout) -> i64 { layout.height / 16 }
// Exact ASCII key lookup against Java UTF-16 compound names, no lossy conversion.
fn key(d: nbt::Document&, n: nbt::Node, name: const u8[:]) -> bool {
    if n.name_len != name.len { return false; }
    for b,i in name { if d.words[n.name + i] != b { return false; } }
    return true;
}
fn layout(name: const u8[:], bytes: const u8[:], present: bool) -> Layout, wire::Error {
    let defaults=vanilla(name);var result=defaults;
    if !present { return result,wire::Error.Ok; }
    let d,used,e=nbt::read(bytes,nbt::Limits {max_bytes:2097152});
    if !nbt::is_ok(e) { return defaults,wire::Error.Nbt {offset:e.offset,reason:e.reason}; }
    let root=nbt::node(d,d.root);
    if root.kind != 10 { return result,wire::Error.Ok; }
    var at=root.first;
    while at != 0 {
        let n=nbt::node(d,at);at=n.next;
        if n.kind==3 {
            if key(d,n,"min_y") {result.min_y=n.bits as! i32;}
            if key(d,n,"height") {result.height=n.bits as! i32;}
            if key(d,n,"logical_height") {result.logical_height=n.bits as! i32;}
        }
        if n.kind==1 {
            if key(d,n,"has_skylight") {result.has_skylight=n.bits!=0;}
            if key(d,n,"has_ceiling") {result.has_ceiling=n.bits!=0;}
        }
        if key(d,n,"ambient_light") {
            if n.kind==5 {result.ambient_light=jmath::gx_f32_from_bits(n.bits as! u32);}
            if n.kind==6 {result.ambient_light=jmath::gx_f64_from_bits(n.bits) as f32;}
            if n.kind==3 {result.ambient_light=(n.bits as! i32) as f32;}
            if n.kind==1 {result.ambient_light=(n.bits as! i8) as f32;}
        }
    }
    // A rejected shape restores only min_y/height, not logical_height or flags.
    if !valid_shape(result.min_y,result.height) { result.min_y=defaults.min_y;result.height=defaults.height; }
    return result,wire::Error.Ok;
}
// Configuration records keep arrival order. Dynamic.add replaces a registry;
// select only its last batch, never concatenate repeated dimension tables.
fn dimensions(records: configuration263::RegistryData[:]) -> Layout[>..], wire::Error {
    var last:i64 = -1;
    for r,i in records {if r.registry=="minecraft:dimension_type" {last=i;}}
    var out:Layout[>..]=[];
    for r,i in records {
        if i != last {continue;}
        for entry in r.entries {
            let value,e=layout(entry.id,entry.data.data,entry.data.present);
            if !wire::is_ok(e) {return [],e;}out.push(value);
        }
    }
    return out,wire::Error.Ok;
}
fn dimension(layouts: Layout[:], id: i32) -> Layout {
    return if id>=0 && id<layouts.len {layouts[id]} else {Layout {}};
}
fn registry_len(records: configuration263::RegistryData[:], name: const u8[:]) -> i64 {
    var n:i64=0;for r in records {if r.registry==name {n=r.entries.len;}}return n;
}
