"""Full pinned chunk.rs with original binary NBT; no codec substitutions."""
import hashlib
import json
from pathlib import Path
import random
import re
import struct
from net_parity import BASE, varint, string


def fixtures():
    rng = random.Random(777263); rows = []; valid = []
    def add(mode, count, data): rows.append(f'{mode} {count} {data.hex() or "-"}')
    section = bytes.fromhex('0100000000010002')
    for count in [0, 1, 2, 24]:
        for tag in [b'\0', bytes.fromhex('0a030001780000000700'), bytes.fromhex('0580000000')]:
            for h in [0, 1, 16, 17]:
                heights = varint(h) + b''.join(varint(i) + varint(2) + struct.pack('>qq', -1, i) for i in range(h))
                data = heights + string(section * count) + varint(1) + bytes.fromhex('3fffc402') + tag
                valid.append(('C', count, data))
    for n in [0, 1, 2047, 2048, 2049]:
        valid.append(('L', 5, b'\x01\x08\x00\x00\x01\x02' + varint(1) + string(bytes([0xf0])*n) + b'\0'))
    for _ in range(250):
        masks = [rng.randbytes(rng.randrange(5)) + b'\0' for i in range(4)]
        data = b''.join(string(m) for m in masks)
        for _ in range(2):
            n = rng.randrange(4)
            data += varint(n) + b''.join(string(rng.randbytes(2048)) for i in range(n))
        valid.append(('L', rng.randrange(40), data))
    for mode, count, data in valid:
        add(mode,count,data);add(mode,count,data+b'\xff')
        for end in sorted({0,1,2,3,4,5,7,len(data)//2,max(0,len(data)-1)}):
            if end<len(data):add(mode,count,data[:end])
    for mode in ['C','L']:
        for _ in range(2500):add(mode,rng.randrange(5),rng.randbytes(rng.randrange(100)))
        for n in [-1,2147483647,17]:add(mode,1,varint(n)+bytes(25))
    # Truncated inner sections and ignored trailing section bytes.
    for end in range(len(section)+1):add('C',1,b'\0'+string(section[:end])+b'\0')
    add('C',1,b'\0'+string(section+b'trailing')+b'\0')
    for mode,count,data in valid:
        if mode=='C':
            add('P',46,struct.pack('>ii',-123,456)+data+bytes(6))
        else:add('P',49,varint(-100)+varint(100)+data)
    for ident in [0,11,12,32,45,46,49,62,120,999]:
        for data in [b'',b'\0',bytes(4),bytes(8)]+[rng.randbytes(rng.randrange(100)) for _ in range(150)]:add('P',ident,data)
    player_packets=[]
    for _ in range(150):
        spawn=varint(rng.randrange(10))+string('minecraft:overworld')+rng.randbytes(8)+varint(rng.randrange(4))
        spawn+=varint(rng.choice([0,1,2,-1,-2147483648,2147483647]))+rng.randbytes(2)
        death=rng.choice([False,True]);spawn+=bytes([255 if death else 0])
        if death:spawn+=string('minecraft:the_nether')+rng.randbytes(8)
        spawn+=varint(rng.randrange(200))+varint(rng.randrange(-64,320))
        login=rng.randbytes(5)+varint(2)+string('minecraft:overworld')+string('minecraft:the_end')
        login+=varint(20)+varint(8)+varint(8)+rng.randbytes(3)+spawn+rng.randbytes(2)
        player_packets.extend([(50,login),(84,spawn+rng.randbytes(1))])
    for flags in range(512):
        # Full float domain, plus exact signed zero/NaN/infinity patterns.
        double=rng.choice([0,1,0x8000000000000000,0x7ff0000000000000,0x7ff8000000000001,rng.getrandbits(64)])
        single=rng.choice([0,0x80000000,0x7f800000,0x7fc00001,rng.getrandbits(32)])
        packet=varint(flags-256)+struct.pack('>Q',double)*3+rng.randbytes(24)+struct.pack('>IIi',single,single,flags)
        player_packets.append((73,packet))
    for ident,data in player_packets:
        add('P',ident,data);add('P',ident,data+b'\0')
        for end in sorted({0,1,2,4,8,16,len(data)//2,len(data)-1}):add('P',ident,data[:end])
    for _ in range(1000):
        pos=rng.randbytes(8)
        packets=[(8,pos+varint(rng.randrange(-2147483648,2147483648))),(38,pos)]
        changes=[rng.randrange(-9223372036854775808,9223372036854775808) for i in range(rng.randrange(12))]
        def varlong(value):
            value &= (1<<64)-1;out=bytearray()
            while value>127:out.append((value&127)|128);value>>=7
            out.append(value);return bytes(out)
        packets.append((86,pos+varint(len(changes))+b''.join(varlong(v) for v in changes)))
        for ident,data in packets:
            add('P',ident,data);add('P',ident,data+b'\0')
            for end in sorted({0,1,7,8,len(data)-1}):
                if end<len(data):add('P',ident,data[:end])
    for length in [-1,2147483647]:add('P',86,bytes(8)+varint(length))
    add('P',86,bytes(8)+b'\x01'+b'\xff'*11)
    return ('\n'.join(rows)+'\n').encode(),len(rows)


def check_chunk_data(args, reference, lock, out, run):
    root=Path(__file__).resolve().parents[2];work=out/'chunk-data';work.mkdir(exist_ok=True)
    sources={}
    for name in ['lib.rs','buf.rs','chunk.rs','ids.rs','packets/mod.rs','packets/play.rs','packets/configuration.rs']:
        raw=(reference/(BASE+name)).read_bytes()
        if hashlib.sha256(raw).hexdigest()!=lock['sources'][BASE+name]:raise SystemExit('Chunk source mismatch: '+name)
        sources[name]=raw.decode()
    nbt=reference/'crates/horizon-world/src/nbt.rs'
    if hashlib.sha256(nbt.read_bytes()).hexdigest()!=lock['sources']['crates/horizon-world/src/nbt.rs']:raise SystemExit('NBT source mismatch')
    template=(root/'goose/tools/chunk_data_oracle.rs.in').read_text().replace('@LIB_TYPES@',sources['lib.rs'][sources['lib.rs'].index('/// Connection protocol states'):])
    for tag,path in {'@NBT_SOURCE@':nbt,'@BUF_SOURCE@':reference/(BASE+'buf.rs'),'@CHUNK_SOURCE@':reference/(BASE+'chunk.rs')}.items():template=template.replace('"'+tag+'"',json.dumps(str(path)))
    # Retain the original packet macros and selected packet declarations;
    # the reduced enum deliberately certifies only the fifteen terrain/player/control variants.
    (work/'packet_macros.rs').write_text(re.sub(r'^pub mod \w+;\n','',sources['packets/mod.rs'],flags=re.M))
    types='#[path='+json.dumps(str(work/'packet_macros.rs'))+'] pub mod packets;\n'
    types+='#[path='+json.dumps(str(reference/(BASE+'ids.rs')))+'] pub mod ids;\n'
    types+='pub mod terrain { use crate::buf::*; use crate::chunk::{ChunkData,LightData}; use crate::nbt::Tag; use crate::packets::Empty; use crate::{packet,packet_enum};\n'
    for name,source in [('BlockUpdate','play'),('ForgetLevelChunk','play'),('SectionBlocksUpdate','play'),('GlobalPos','play'),('SpawnInfo','play'),('Login','play'),('Respawn','play'),('PlayerPosition','play'),('ChunkBatchFinished','play'),('LevelChunkWithLight','play'),('LightUpdate','play'),('KeepAlive','configuration'),('Ping','configuration'),('Disconnect','configuration')]:
        m=re.search(r'packet!\(\s*(?:///[^\n]*\n\s*)*'+name+r'\s*\{[^}]*\}\s*\);',sources['packets/'+source+'.rs'])
        if not m:raise SystemExit('Missing original packet '+name)
        types+=m.group()+'\n'
    rows=[]
    for name in ['BlockUpdate','ForgetLevelChunk','SectionBlocksUpdate','Login','Respawn','PlayerPosition','BundleDelimiter','ChunkBatchStart','ChunkBatchFinished','KeepAlive','Ping','Disconnect','StartConfiguration','LevelChunkWithLight','LightUpdate']:
        rows.append(re.search(r'^    '+name+r'\([^)]*\) = [A-Z_]+,',sources['packets/play.rs'],re.M).group())
    types+='packet_enum!(Clientbound, crate::ids::play::clientbound {\n'+'\n'.join(rows)+'\n}); }\n'
    template=template.replace('@PLAY_TYPES@',types)
    (work/'oracle.rs').write_text(template)
    run([args.rustc,'--edition=2024','--test','-O',work/'oracle.rs','-o',work/'original-tests'])
    tests=run([work/'original-tests','chunk::tests::']).decode();print(tests,end='')
    run([args.rustc,'--edition=2024','-O',work/'oracle.rs','-o',work/'oracle'])
    data,count=fixtures();(work/'fixtures.txt').write_bytes(data)
    expected=run([work/'oracle'],input=data);(work/'expected.txt').write_bytes(expected)
    for mode,flags in [('debug',['--debug']),('release',[])]:
        exe=work/f'goose-{mode}';run([args.goose,'build',*flags,'-o',exe,root/'goose/chunk_data_probe.goose'])
        actual=run([exe],input=data);(work/f'{mode}.txt').write_bytes(actual)
        if actual!=expected:
            a,b=actual.splitlines(),expected.splitlines();i=next((i for i,p in enumerate(zip(a,b)) if p[0]!=p[1]),min(len(a),len(b)))
            raise SystemExit(f'Chunk data {mode} mismatch at output {i+1}: {a[i][:200]!r} vs {b[i][:200]!r}')
    report={'module':'net/chunk-data','revision':lock['revision'],'fixture_lines':count,'matched_builds':['goose-debug','goose-release'],'output_sha256':hashlib.sha256(expected).hexdigest(),'original_tests':tests,'scope':'Fifteen selected original terrain/player/control play codec variants, plus full original chunk.rs: column and light canonical bytes, error offsets, sections and all expanded light bytes. Original NBT/buf unchanged.','adaptations':['NBT fields own canonical network bytes.','block is a Goose keyword: block_light field.','Linear layer expansion uses borrowed views, copying each layer into the final owner only.']}
    (out/'chunk-data-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {count} chunk data fixtures match Rust in debug/release.')
    return report
