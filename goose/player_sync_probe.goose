import std;
import os;
import net.player_sync;
fn hex(data: const u8[:]) -> u8[>..] { let digits: const u8[:]="0123456789abcdef"; var out: u8[>..]=[]; for b in data { out.push(digits[b>>4]);out.push(digits[b&15]); } return out; }
fn number(r: wire::Reader&) -> f64 { let v,e=wire::read_i64(r);assert(wire::is_ok(e));return jmath::gx_f64_from_bits(v as! u64); }
fn single(r: wire::Reader&) -> f32 { let v,e=wire::read_u32(r);assert(wire::is_ok(e));return jmath::gx_f32_from_bits(v); }
fn main() {
    var input:u8[>..]=[];read_stdin(input);var sender=player_sync263::PositionSender {};
    for line in split(input,'\n') {
        if line.len==0 {continue;} let p=split(line,' ');
        if p[0]=="T" { for i in 65536 { print(jmath::gx_f32_bits(trig263::table(i))); } continue; }
        if p[0]=="R" { sender=player_sync263::PositionSender {};continue; }
        var bytes:u8[>..]=[];var i:i64=0;
        while i<p[1].len {let b,ok=parse_int(p[1][i..i+2],16);assert(ok);bytes.push(b as! u8);i+=2;}
        var r=wire::reader(bytes);
        if p[0]=="S" { let x=number(r);print(jmath::gx_f32_bits(trig263::sin(x))," ",jmath::gx_f32_bits(trig263::cos(x))); }
        else if p[0]=="M" {
            var state=player_sync263::State {};state.x=number(r);state.y=number(r);state.z=number(r);state.yaw=single(r);state.pitch=single(r);
            let ground,e=wire::read_bool(r);let collision,f=wire::read_bool(r);assert(wire::is_ok(e)&&wire::is_ok(f));
            let result=player_sync263::movement(sender,state,ground,collision);let text=hex(result);print(text);
        } else {
            assert(p[0]=="V");let v=trig263::Vec3 {x:number(r),y:number(r),z:number(r)};
            let oy=single(r);let op=single(r);let ny=single(r);let np=single(r);
            let delta,e=play263::read_vector(r);let flags,f=wire::read_i32(r);assert(wire::is_ok(e)&&wire::is_ok(f));
            let packet=play263::PlayerPosition {delta_bits:delta,relatives:flags};
            let result=player_sync263::teleport_velocity(v,oy,op,ny,np,packet);
            let a=if result.x!=result.x {0x7ff8000000000000 as u64}else{jmath::gx_f64_bits(result.x)};
            let b=if result.y!=result.y {0x7ff8000000000000 as u64}else{jmath::gx_f64_bits(result.y)};
            let c=if result.z!=result.z {0x7ff8000000000000 as u64}else{jmath::gx_f64_bits(result.z)};
            print(a," ",b," ",c);
        }
    }
}
