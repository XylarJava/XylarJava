"""Binary NBT read/write parity against the complete unchanged Rust source."""
import hashlib
import json
from pathlib import Path
import random
import struct


def mutf(units):
    out = bytearray()
    for c in units:
        if 1 <= c <= 127: out.append(c)
        elif c <= 2047: out.extend([192 | (c >> 6), 128 | (c & 63)])
        else: out.extend([224 | (c >> 12), 128 | ((c >> 6) & 63), 128 | (c & 63)])
    return struct.pack('>H', len(out)) + out


def fixtures():
    rng = random.Random(0x4e4254323633)
    lines = []
    def add(data, named=False, limits=(67108864, 1000000, 512)):
        lines.append(f'{"N" if named else "R"} {limits[0]} {limits[1]} {limits[2]} {data.hex() or "-"}')
    samples = [b'\0', b'\x0a\0', b'\x09\0\0\0\0\0', bytes.fromhex('0a0100017801010001780200'),
               bytes.fromhex('090a0000000203000000000001000100000200')]
    for kind, size in [(1,1),(2,2),(3,4),(4,8),(5,4),(6,8)]:
        for _ in range(150): samples.append(bytes([kind])+rng.randbytes(size))
    for bits in [0,0x80000000,0x7f800000,0x7f800001,0x7fffffff,0xff800001]:
        samples.append(b'\x05'+bits.to_bytes(4,'big'))
    for bits in [0,1<<63,0x7ff0000000000000,0x7ff0000000000001,0xfff8000000000123]:
        samples.append(b'\x06'+bits.to_bytes(8,'big'))
    for units in [[],[0],[0xd800],[0xdc00],[0,0xd800,0xdc00,0xdfff],list(range(256))]:
        samples.append(b'\x08'+mutf(units))
    for length in [0,1,2,3,31,32,127,128,4096]:
        for kind, size in [(7,1),(11,4),(12,8)]:
            samples.append(bytes([kind])+struct.pack('>i',length)+rng.randbytes(size*length))
    def tree(depth=0):
        kind=rng.randrange(1,13) if depth<4 else rng.choice([1,3,7,8])
        if kind in (1,2,3,4,5,6): return bytes([kind])+rng.randbytes({1:1,2:2,3:4,4:8,5:4,6:8}[kind])
        if kind in (7,11,12):
            n=rng.randrange(16); return bytes([kind])+struct.pack('>i',n)+rng.randbytes(n*{7:1,11:4,12:8}[kind])
        if kind==8: return b'\x08'+mutf([rng.randrange(65536) for _ in range(rng.randrange(12))])
        if kind==9:
            # Compound wrappers encode heterogeneous 26.3 lists, including
            # nested empty-name compounds that must be wrapped again on write.
            items=[tree(depth+1) for _ in range(rng.randrange(6))]
            return b'\x09\x0a'+struct.pack('>i',len(items))+b''.join(v[:1]+b'\0\0'+v[1:]+b'\0' for v in items)
        items=[]
        for _ in range(rng.randrange(6)):
            value=tree(depth+1); key=[rng.choice([0,97,98,0xd800,0xdc00,65535]) for _ in range(rng.randrange(3))]
            items.append(value[:1]+mutf(key)+value[1:])
        return b'\x0a'+b''.join(items)+b'\0'
    samples += [tree() for _ in range(2000)]
    for data in samples:
        add(data)
        add(data[:1]+mutf([0,0xd800,0xdc00])+data[1:] if data[0] else data, True)
        for size in sorted({0,1,2,len(data)//2,max(0,len(data)-1)}):
            if size<len(data): add(data[:size])
        for limits in [(0,1000000,512),(len(data)-1,1000000,512),(len(data),0,512),(len(data),1,0)]:
            if limits[0]>=0: add(data,limits=limits)
    # Every two-byte sequence through the modified-UTF decoder.
    for n in range(65536): add(b'\x08\0\2'+n.to_bytes(2,'big'))
    for depth in [0,1,2,511,512,513]:
        data=b'\x01\0'
        for _ in range(depth): data=b'\x09'+data[:1]+b'\0\0\0\1'+data[1:]
        for limit in [0,1,2,512,513]: add(data,limits=(67108864,1000000,limit))
    for raw in ['0a0002ffff00','0a0002ffff030001780000000700','0a0009ff','0a000003000278ff0000000700']:
        add(bytes.fromhex(raw),True)
    for _ in range(5000): add(rng.randbytes(rng.randrange(64)))
    for kind in [7,9,11,12]:
        for n in [-2147483648,-1,0,1,2147483647]:
            for element in ([0,1,12,13,255] if kind==9 else [0]):
                add(bytes([kind])+ (bytes([element]) if kind==9 else b'') +struct.pack('>i',n))
    return ('\n'.join(lines)+'\n').encode(),len(lines)


def check_nbt(args, reference, lock, out, run):
    root=Path(__file__).resolve().parents[2]
    name='crates/horizon-world/src/nbt.rs'
    source=reference/name
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    if digest!=lock['sources'][name]: raise SystemExit('NBT reference hash mismatch')
    work=out/'nbt';work.mkdir(exist_ok=True)
    template=(root/'goose/tools/nbt_oracle.rs.in').read_text().replace('"@NBT_SOURCE@"',json.dumps(str(source)))
    (work/'oracle.rs').write_text(template)
    run([args.rustc,'--edition=2024','-O',work/'oracle.rs','-o',work/'oracle'])
    run([args.rustc,'--edition=2024','--test','-O',source,'-o',work/'original-tests'])
    original_tests=run([work/'original-tests','--skip','snbt','--skip','streamed_lists','--skip','templates_read']).decode()
    print(original_tests,end='')
    data,count=fixtures();(work/'fixtures.txt').write_bytes(data)
    expected=run([work/'oracle'],input=data);(work/'expected.txt').write_bytes(expected)
    for mode,flags in [('debug',['--debug']),('release',[])]:
        exe=work/f'goose-{mode}'
        run([args.goose,'build',*flags,'-o',exe,root/'goose/nbt_probe.goose'])
        actual=run([exe],input=data);(work/f'{mode}.txt').write_bytes(actual)
        if actual!=expected:
            a,b=actual.splitlines(),expected.splitlines()
            i=next((i for i,p in enumerate(zip(a,b)) if p[0]!=p[1]),min(len(a),len(b)))
            raise SystemExit(f'NBT {mode} mismatch at fixture {i+1}: {a[i][:200]!r} vs {b[i][:200]!r}')
    report={'module':'data/nbt-binary','revision':lock['revision'],'source_sha256':digest,'fixture_lines':count,
            'matched_builds':['goose-debug','goose-release'],'output_sha256':hashlib.sha256(expected).hexdigest(),
            'original_binary_tests':original_tests,
            'scope':'Binary named/network NBT reader and writer; lossless UTF-16, sorted compounds, mixed lists and quotas',
            'adaptations':['Single growable u64 arena holds node headers, links and variable data instead of recursive Rust enums/BTreeMap.',
                           'Compound lookup uses sorted linked entries; duplicate keys retain their final value.'],
            'pending':['SNBT','streaming-list visitor API','limits presets and string convenience APIs']}
    (out/'nbt-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {count} binary NBT fixtures; Rust/Goose bytes, consumed lengths and errors match in debug/release.')
    return report
