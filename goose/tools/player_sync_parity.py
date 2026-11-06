"""Pinned PositionSender/teleport_velocity and quantized sine table comparisons."""
import hashlib
import json
from pathlib import Path
import random
import re
import struct
from net_parity import BASE

def fixtures():
    rng=random.Random(77726351);lines=['T']
    def add(kind,data):lines.append(kind+' '+data.hex())
    for _ in range(4000):add('S',rng.randbytes(8))
    for x in [0.,-0.,float('nan'),float('inf'),-float('inf'),1e300,-1e300]:add('S',struct.pack('>d',x))
    for flags in range(512):
        for _ in range(16):
            velocities=[rng.uniform(-30,30) for _ in range(3)]
            angles=[rng.uniform(-720,720) for _ in range(4)]
            delta=[rng.uniform(-3,3) for _ in range(3)]
            add('V',struct.pack('>3d4f3di',*velocities,*angles,*delta,flags))
    for value in [0.,-0.,1e300,-1e300,float('inf'),-float('inf'),float('nan')]:
        for flags in range(512):add('V',struct.pack('>3d4f3di',value,value,value,90.,-90.,0.,0.,0.,0.,0.,flags))
    for _ in range(200):
        lines.append('R');pos=[0.,0.,0.];rot=[0.,0.];ground=False;collision=False
        for tick in range(100):
            if rng.random()<.25:pos[rng.randrange(3)]+=rng.choice([0.,0.0001,0.0002,0.0002000000001,1.,-2.])
            if rng.random()<.2:rot[rng.randrange(2)]+=rng.choice([0.,1.,-2.])
            if rng.random()<.1:ground=not ground
            if rng.random()<.1:collision=not collision
            add('M',struct.pack('>3d2fBB',*pos,*rot,ground,collision))
    # Repeated idle frames exercise the twentieth-update reminder precisely.
    lines.append('R')
    for _ in range(100):add('M',bytes(34))
    return ('\n'.join(lines)+'\n').encode(),len(lines)

def check_player_sync(args,reference,lock,out,run):
    root=Path(__file__).resolve().parents[2];work=out/'player-sync';work.mkdir(exist_ok=True)
    paths={'lib':BASE+'lib.rs','buf':BASE+'buf.rs','ids':BASE+'ids.rs','macros':BASE+'packets/mod.rs','play':BASE+'packets/play.rs',
           'sync':'crates/horizon-client/src/player_sync.rs','trig':'crates/horizon-core/src/geometry/trig.rs','geometry':'crates/horizon-core/src/geometry/mod.rs','nbt':'crates/horizon-world/src/nbt.rs'}
    sources={}
    for key,path in paths.items():
        raw=(reference/path).read_bytes()
        if hashlib.sha256(raw).hexdigest()!=lock['sources'][path]:raise SystemExit('Sync source mismatch '+path)
        sources[key]=raw.decode()
    packet=re.sub(r'^pub mod \w+;\n','',sources['macros'],flags=re.M)
    packet+='\npub mod play { use crate::buf::*; use crate::{packet,packet_enum};\n'
    names=['PlayerPosition','MovePlayerPos','MovePlayerPosRot','MovePlayerRot','MovePlayerStatus']
    for name in names:
        packet+=re.search(r'packet!\(\s*(?:///[^\n]*\n\s*)*'+name+r'\s*\{[^}]*\}\s*\);',sources['play']).group()+'\n'
    packet+=re.search(r'pub mod relative \{[^}]*\}',sources['play']).group()+'\n'
    packet+='packet_enum!(Serverbound,crate::ids::play::serverbound {\n'
    for name in names[1:]:packet+=re.search(r'^    '+name+r'\([^)]*\) = [A-Z_]+,',sources['play'],re.M).group()+'\n'
    packet+='});}\n';(work/'packets.rs').write_text(packet)
    methods='\n'.join(re.search(r'    pub (?:const )?fn '+name+r'\(.*?^    \}',sources['geometry'],re.S|re.M).group() for name in ['new','rotate_x','rotate_y'])
    template=(root/'goose/tools/player_sync_oracle.rs.in').read_text().replace('@LIB_TYPES@',sources['lib'][sources['lib'].index('/// Connection protocol states'):]).replace('@VEC_METHODS@',methods)
    for key in ['nbt','buf','ids','sync','trig']:template=template.replace('"@'+key.upper()+'_SOURCE@"',json.dumps(str(reference/paths[key])))
    template=template.replace('"@PACKETS_SOURCE@"',json.dumps(str(work/'packets.rs')))
    (work/'oracle.rs').write_text(template);run([args.rustc,'--edition=2024','-O',work/'oracle.rs','-o',work/'oracle'])
    data,count=fixtures();(work/'fixtures.txt').write_bytes(data);expected=run([work/'oracle'],input=data);(work/'expected.txt').write_bytes(expected)
    for mode,flags in [('debug',['--debug']),('release',[])]:
        exe=work/f'goose-{mode}';run([args.goose,'build',*flags,'-o',exe,root/'goose/player_sync_probe.goose'])
        actual=run([exe],input=data);(work/f'{mode}.txt').write_bytes(actual)
        if actual!=expected:
            a,b=actual.splitlines(),expected.splitlines();i=next((i for i,p in enumerate(zip(a,b)) if p[0]!=p[1]),min(len(a),len(b)))
            raise SystemExit(f'Player sync {mode} mismatch at output {i+1}: {a[i][:200]!r} vs {b[i][:200]!r}')
    report={'module':'player-sync','revision':lock['revision'],'fixture_lines':count,'table_entries':65536,'matched_builds':['debug','release'],
            'output_sha256':hashlib.sha256(expected).hexdigest(),'scope':'Full original player_sync.rs with original movement packet codecs, trig source and extracted unchanged Vec3 constructor/rotations. Movement sender is not yet attached to a gameplay tick/input loop.',
            'adaptations':['Goose sine table is initialized once per worker, not process-shared OnceLock.','Arithmetic velocity NaNs are normalized in output comparison; finite/infinite/signed-zero results compare bitwise.','Diagnostic teleport acknowledgement pose is separately checked by independent stream fixtures.']}
    (out/'player-sync-report.json').write_text(json.dumps(report,indent=2)+'\n');print(f'PASS: {count} player sync fixtures plus all 65536 sine entries.');return report
