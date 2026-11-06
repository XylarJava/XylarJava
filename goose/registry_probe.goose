import std;
import os;
import net.registry;
fn main() {
    var input:u8[>..]=[];read_stdin(input);
    for line in split(input,'\n') {
        if line.len==0 {continue;}let p=split(line,' ');var bytes:u8[>..]=[];
        if p[1]!="-" {var i:i64=0;while i<p[1].len {let b,ok=parse_int(p[1][i..i+2],16);assert(ok);bytes.push(b as! u8);i+=2;}}
        let d,e=registry263::layout(p[0],bytes,p[1]!="-");
        if !wire::is_ok(e) {let msg=wire::error_message(e);print("ERR ",msg);continue;}
        print(d.min_y," ",d.height," ",d.logical_height," ",if d.has_skylight {1}else{0}," ",if d.has_ceiling {1}else{0}," ",jmath::gx_f32_bits(d.ambient_light)," ",registry263::sections(d));
    }
}
