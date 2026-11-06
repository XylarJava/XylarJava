"""Complete configuration packet codecs, compared to unchanged pinned bodies."""
import hashlib
import json
from pathlib import Path
import random
import struct
from net_parity import BASE, varint, string


def fixtures():
    rng=random.Random(0x434647373737);lines=[];packets=[]
    def add(d,i,data):lines.append(f'{d} {i} {data.hex() or "-"}')
    nbt=[b'\0',b'\x01\xff',b'\x08\0\3bye',bytes.fromhex('0a030001780000000700'),bytes.fromhex('0580000000'),bytes.fromhex('05ff800001')]
    for d,ids in [('C',[3,6,18]),('S',[3,9])]:
        for i in ids:packets.append((d,i,b''))
    for d,i in [('C',0),('C',1),('C',11),('C',12),('C',20),('S',1),('S',2)]:
        for s in ['', 'minecraft:brand','😀'*8, 'x'*32767,'x'*32768,b'\xff']:
            tail=b'payload' if i==1 and d=='C' or i==2 and d=='S' else string(b'cookie') if i==11 else varint(25565) if i==12 else b'\0' if i==1 and d=='S' else b''
            packets.append((d,i,string(s)+tail))
    for d,i,size in [('C',4,8),('C',5,4),('S',4,8),('S',5,4)]:
        for _ in range(30):packets.append((d,i,rng.randbytes(size)))
    for tag in nbt:
        packets.extend([('C',2,tag),('C',19,tag),('S',8,string('test:click')+string(tag)),('S',8,string('test:click')+string(tag+b'extra'))])
        for present in [0,1,255]:
            optional=bytes([present])+(tag if present else b'')
            packets.append(('C',7,string('minecraft:worldgen/biome')+varint(1)+string('minecraft:plains')+optional))
            packets.append(('C',9,bytes(range(16))+string('https://example.invalid/pack.zip')+string('abc')+b'\x01'+optional))
            packets.append(('C',17,varint(1)+b'\0'+tag+string('https://example.invalid/')))
    for present in [0,1,255]:packets.append(('C',8,bytes([present])+(bytes(range(16)) if present else b'')))
    for main_hand in [0,1,-1,2]:
        for distance in [0,1,12,127,128,255]:
            packets.append(('S',0,string('en_us')+bytes([distance])+varint(0)+b'\x01\x7f'+varint(main_hand)+b'\0\1'+varint(0)))
    for n in [0,1,2,3,-1,2147483647]:
        packs=varint(n)+b''.join(string('minecraft')+string('core')+string('26.3') for _ in range(max(0,min(n,3))))
        packets.extend([('C',15,packs),('S',7,packs)])
        packets.append(('C',13,varint(n)+string('minecraft:vanilla')))
        packets.append(('C',7,string('minecraft:test')+varint(n)))
    for count in [0,1,3]:
        tags=varint(count)+b''.join(string('test:tag')+varint(3)+varint(1)+varint(-1)+varint(300) for _ in range(count))
        packets.append(('C',14,varint(1)+string('minecraft:block')+tags))
        links=varint(count)+b''.join(b'\x01'+varint(i)+string('https://example.invalid/') for i in range(count))
        packets.append(('C',17,links))
    packets.append(('S',6,bytes(range(16))+varint(1)))
    for d,i,data in packets:
        add(d,i,data);add(d,i,data+b'\0')
        for end in sorted({0,1,2,3,4,len(data)//2,max(0,len(data)-1)}):
            if end<len(data):add(d,i,data[:end])
    for d,ids in [('C',range(21)),('S',range(10))]:
        for i in ids:
            for _ in range(250):add(d,i,rng.randbytes(rng.randrange(96)))
    return ('\n'.join(lines)+'\n').encode(),len(lines)


def check_configuration(args,reference,lock,out,run):
    root=Path(__file__).resolve().parents[2];work=out/'configuration';work.mkdir(exist_ok=True)
    snapshot=work/'reference';(snapshot/'packets').mkdir(parents=True,exist_ok=True)
    names=['lib.rs','buf.rs','ids.rs','packets/mod.rs','packets/handshake.rs','packets/status.rs','packets/configuration.rs']
    sources={}
    for name in names:
        raw=(reference/(BASE+name)).read_bytes()
        if hashlib.sha256(raw).hexdigest()!=lock['sources'][BASE+name]:raise SystemExit('Configuration source mismatch: '+name)
        sources[name]=raw.decode()
    (snapshot/'buf.rs').write_text(sources['buf.rs'].replace('use horizon_world::nbt::{self, Tag};','use crate::nbt::{self, Tag};'))
    packets=sources['packets/mod.rs']
    for name in ['login','play','recipes']:packets=packets.replace(f'pub mod {name};\n','')
    (snapshot/'packets/mod.rs').write_text(packets)
    for name in ['configuration','handshake','status']:(snapshot/f'packets/{name}.rs').write_text(sources[f'packets/{name}.rs'])
    template=(root/'goose/tools/configuration_oracle.rs.in').read_text()
    template=template.replace('@LIB_TYPES@',sources['lib.rs'][sources['lib.rs'].index('/// Connection protocol states'):])
    for tag,path in {'@NBT_SOURCE@':reference/'crates/horizon-world/src/nbt.rs','@BUF_SOURCE@':snapshot/'buf.rs','@IDS_SOURCE@':reference/(BASE+'ids.rs'),'@PACKETS_SOURCE@':snapshot/'packets/mod.rs'}.items():
        template=template.replace('"'+tag+'"',json.dumps(str(path)))
    (work/'oracle.rs').write_text(template)
    run([args.rustc,'--edition=2024','--test','-O',work/'oracle.rs','-o',work/'original-tests'])
    tests=run([work/'original-tests','packets::configuration::tests::']).decode();print(tests,end='')
    run([args.rustc,'--edition=2024','-O',work/'oracle.rs','-o',work/'oracle'])
    data,count=fixtures();(work/'fixtures.txt').write_bytes(data)
    expected=run([work/'oracle'],input=data);(work/'expected.txt').write_bytes(expected)
    for mode,flags in [('debug',['--debug']),('release',[])]:
        exe=work/f'goose-{mode}';run([args.goose,'build',*flags,'-o',exe,root/'goose/configuration_probe.goose'])
        actual=run([exe],input=data);(work/f'{mode}.txt').write_bytes(actual)
        if actual!=expected:
            a,b=actual.splitlines(),expected.splitlines();i=next((i for i,p in enumerate(zip(a,b)) if p[0]!=p[1]),min(len(a),len(b)))
            raise SystemExit(f'Configuration {mode} mismatch at fixture {i+1}: {a[i][:200]!r} vs {b[i][:200]!r}')
    report={'module':'net/configuration','revision':lock['revision'],'fixture_lines':count,'source_sha256':lock['sources'][BASE+'packets/configuration.rs'],
            'matched_builds':['goose-debug','goose-release'],'output_sha256':hashlib.sha256(expected).hexdigest(),'original_tests':tests,
            'scope':'All 19 clientbound and 10 serverbound configuration codec variants including NBT, unknown packet passthrough and full consumption',
            'adaptations':['NBT Tag fields use validated canonical network bytes.','UTF-8/list codecs and packet definitions remain original in the oracle.','Original round-trip fixture text::literal helper produces a simple NBT String; text-component construction is not certified.']}
    (out/'configuration-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {count} configuration fixtures; canonical packet bodies and errors match Rust in debug/release.')
    return report
