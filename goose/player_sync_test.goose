import std;
import net.player_sync;
fn main() {
    var s = player_sync263::State {};
    let login = play263::Login { player_id: 42, spawn: play263::SpawnInfo { dimension_type: 3, game_type: 1 } };
    player_sync263::login(s,login);
    assert(s.logged_in && s.player_id == 42 && s.dimension_type == 3 && s.game_type == 1 && !s.positioned);
    let p = play263::PlayerPosition { teleport: 7, position_bits: [(-9223372036854775807 - 1),0x3ff0000000000000,0x4000000000000000], yaw_bits: 0x80000000, pitch_bits: 0x80000000 };
    let ack = player_sync263::teleport(s,p);
    assert(s.positioned && s.teleports == 1);
    assert(jmath::gx_f64_bits(s.x)==0x8000000000000000 && s.y==1.0 && s.z==2.0);
    assert(jmath::gx_f32_bits(s.yaw)==0x80000000 && jmath::gx_f32_bits(s.pitch)==0);
    assert(ack.len==34 && ack[0]==0 && ack[1]==7 && ack[2]==128 && ack[26]==128 && ack[30]==0);
    let n = play263::PlayerPosition { teleport: -1, pitch_bits: 0x7fc00001 };
    let nan_ack = player_sync263::teleport(s,n);
    assert(s.pitch != s.pitch && nan_ack.len==38 && s.teleports==2);
    let high = play263::PlayerPosition { pitch_bits: 0x7f800000 };
    let high_ack = player_sync263::teleport(s,high); assert(s.pitch==90.0);
    let low = play263::PlayerPosition { pitch_bits: 0xff800000 };
    let low_ack = player_sync263::teleport(s,low); assert(s.pitch== -90.0);
    var moving=player_sync263::State {velocity:trig263::Vec3 {x:1.0,y:2.0,z:3.0}};
    let rotation=play263::PlayerPosition {delta_bits:[0x3fe0000000000000,(-4620693217682128896),0x3ff0000000000000],yaw_bits:0x42b40000,relatives:480};
    let rotated_ack=player_sync263::teleport(moving,rotation);
    assert(moving.velocity.x== -2.5 && moving.velocity.y==1.5 && moving.velocity.z==2.0);
    var sender=player_sync263::PositionSender {};
    var stationary=player_sync263::State {};
    for i in 19 {let none=player_sync263::movement(sender,stationary,false,false);assert(none.len==0);}
    let reminder=player_sync263::movement(sender,stationary,false,false);assert(reminder.len==26 && reminder[0]==30);
    let teleport=player_sync263::teleport(stationary,p);
    let next=player_sync263::movement(sender,stationary,false,false);assert(next.len>0 && next[0]==30);
    print("Player login state, signed zeros, NaN/infinite pitch and teleport acknowledgement passed.");
}
