"""Dimension layout subset against pinned Dynamic.dimension and binary NBT."""
import hashlib,json,random,struct
from pathlib import Path

def fixtures():
    rng=random.Random(263777);rows=[]
    def field(name,kind,value):
        name=name.encode();return bytes([kind])+struct.pack('>H',len(name))+name+value
    def add(name,fields):rows.append(name+' '+(b'\x0a'+b''.join(fields)+b'\0').hex())
    names=['minecraft:overworld','minecraft:the_nether','minecraft:the_end','minecraft:overworld_caves','pack:custom']
    for name in names:
        rows.extend([name+' -',name+' 00',name+' 0a',name+' 08000178'])
        for y in [-2147483648,-2048,-2032,-64,-1,0,16,1024,2032,2147483647]:
            for h in [-1,0,8,16,100,256,384,4064,2147483632]:
                add(name,[field('min_y',3,struct.pack('>i',y)),field('height',3,struct.pack('>i',h)),field('logical_height',3,struct.pack('>i',-7))])
    for _ in range(6000):
        fields=[]
        for name in ['min_y','height','logical_height','has_skylight','has_ceiling','ambient_light']:
            if rng.random()<.25:continue
            kind=rng.choice([1,2,3,4,5,6,8]);size={1:1,2:2,3:4,4:8,5:4,6:8,8:0}[kind]
            value=rng.randbytes(size) if size else b'\0\3abc'
            fields.append(field(name,kind,value))
        add(rng.choice(names),fields)
    for kind,value in [(5,bytes.fromhex('80000000')),(6,bytes.fromhex('8000000000000000')),(5,bytes.fromhex('7f800001')),(6,bytes.fromhex('7ff8000000000001'))]:add('pack:test',[field('ambient_light',kind,value)])
    return ('\n'.join(rows)+'\n').encode(),len(rows)

def check_registry(args,reference,lock,out,run):
    root=Path(__file__).resolve().parents[2];work=out/'registry';work.mkdir(exist_ok=True)
    path='crates/horizon-protocol/src/registry.rs';nbt='crates/horizon-world/src/nbt.rs'
    for name in [path,nbt]:
        if hashlib.sha256((reference/name).read_bytes()).hexdigest()!=lock['sources'][name]:raise SystemExit('Registry source mismatch '+name)
    source=(reference/path).read_text();body=source[source.index('/// Registries synchronized'):source.index('#[cfg(test)]')]
    template=(root/'goose/tools/registry_oracle.rs.in').read_text().replace('@REGISTRY_BODY@',body).replace('"@NBT_SOURCE@"',json.dumps(str(reference/nbt)))
    (work/'oracle.rs').write_text(template);run([args.rustc,'--edition=2024','-O',work/'oracle.rs','-o',work/'oracle'])
    data,count=fixtures();(work/'fixtures.txt').write_bytes(data);expected=run([work/'oracle'],input=data);(work/'expected.txt').write_bytes(expected)
    for mode,flags in [('debug',['--debug']),('release',[])]:
        exe=work/f'goose-{mode}';run([args.goose,'build',*flags,'-o',exe,root/'goose/registry_probe.goose']);actual=run([exe],input=data);(work/f'{mode}.txt').write_bytes(actual)
        if actual!=expected:
            a,b=actual.splitlines(),expected.splitlines();i=next((i for i,p in enumerate(zip(a,b)) if p[0]!=p[1]),min(len(a),len(b)))
            raise SystemExit(f'Registry {mode} mismatch {i+1}: {a[i]!r} vs {b[i]!r}')
    report={'module':'registry-dimension-layout','fixture_lines':count,'revision':lock['revision'],'matched_builds':['debug','release'],'output_sha256':hashlib.sha256(expected).hexdigest(),'scope':'min_y, height, logical_height, skylight, ceiling, ambient_light bits and section count; unchanged Dynamic/DimensionType source and NBT. Names/default_clock/general registry APIs are not certified.'}
    (out/'registry-report.json').write_text(json.dumps(report,indent=2)+'\n');print(f'PASS: {count} dimension layout fixtures.');return report
