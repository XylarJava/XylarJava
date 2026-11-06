"""Protocol 777 primitive, handshake/status and section-wire parity."""
import hashlib
import json
import random
from pathlib import Path

BASE = 'crates/horizon-protocol/src/'


def varint(value):
    value &= (1 << 32) - 1
    out = bytearray()
    while value >= 128:
        out.append((value & 127) | 128)
        value >>= 7
    out.append(value)
    return bytes(out)


def string(value):
    if isinstance(value, str):
        value = value.encode()
    return varint(len(value)) + value


def encode_palette(blocks, global_bits, leading, palette, values):
    if leading == 0:
        return bytes([0]) + varint(palette[0])
    local = leading <= (8 if blocks else 3)
    bits = max(4, leading) if blocks and local else leading if local else max(global_bits, leading)
    out = bytearray([leading])
    if local:
        out.extend(varint(len(palette)))
        for value in palette:
            out.extend(varint(value))
    per = 64 // bits
    for start in range(0, len(values), per):
        word = 0
        for offset, value in enumerate(values[start:start + per]):
            word |= value << (offset * bits)
        out.extend((word & ((1 << 64) - 1)).to_bytes(8, 'big'))
    return bytes(out)


def fixtures():
    rng = random.Random(0x50524F544F373737)
    commands = []

    def add(fields, data):
        commands.append(' '.join(map(str, fields)) + ' ' + (data.hex() or '-'))

    for op in range(20):
        for _ in range(200):
            data = rng.randbytes(rng.randrange(41))
            add(['R', op, rng.choice([0, 1, 2, 3, 8, 16, 255, 1000]), rng.randrange(len(data) + 1)], data)
    for value in [0, 1, 127, 128, 255, 25565, 16383, 16384, 2097151, 2147483647, -1, -2147483648]:
        data = varint(value)
        for size in range(len(data) + 1):
            add(['R', 8, 0, 0], data[:size])
        add(['R', 15, 255, 0], data)
    for data in [b'\xff' * 5, b'\xff' * 6, b'\x80' * 10, b'\xff' * 10, b'\xff' * 11]:
        add(['R', 8, 0, 0], data)
        add(['R', 9, 0, 0], data)
    for value in [0, 1, -1, -(1 << 63), (1 << 63) - 1]:
        raw = (value & ((1 << 64) - 1)).to_bytes(8, 'big')
        for size in range(9):
            add(['R', 7, 0, 0], raw[:size])
    # All two-byte sequences, valid or malformed, through a framed string.
    for number in range(65536):
        add(['R', 11, 200, 0], string(number.to_bytes(2, 'big')))
    for text in ['', 'hello', 'héllo', '😀', '😀' * 8, '𝟙x', 'a\x00z', '日本語', '١٢٣']:
        for maximum in [0, 1, 2, 3, 15, 16, 255]:
            add(['R', 11, maximum, 0], string(text))
    for _ in range(3000):
        raw = rng.randbytes(rng.randrange(12))
        add(['R', 11, 100, 0], string(raw))
    for lead in [0xe0, 0xed, 0xef, 0xf0, 0xf1, 0xf4]:
        for second in [0, 0x7f, 0x80, 0x8f, 0x90, 0x9f, 0xa0, 0xbf, 0xc0, 0xff]:
            for size in [1, 2, 3, 4]:
                add(['R', 11, 100, 0], string(bytes([lead, second, 0x80, 0x80])[:size]))
    for count in range(8):
        for available in range(20):
            add(['R', 14, 0, 0], varint(count) + rng.randbytes(available))
    # Complete and incomplete handshake/status packets. Unknown packets retain bytes.
    for host in ['localhost', '', '例え.test', '😀' * 127, '😀' * 128, 'a' * 256]:
        for protocol in [0, 777, -1]:
            for intent in [1, 2, 3, -1]:
                payload = varint(protocol) + string(host) + b'\x63\xdd' + varint(intent)
                for end in sorted({0, 1, 2, max(0, len(payload) - 3), len(payload) - 1, len(payload)}):
                    add(['H', 0], payload[:end])
                add(['H', 0], payload + b'\x00')
    for direction in [0, 1]:
        for payload in [b'', b'\x00', string('{"version":{"protocol":777}}'), string('😀' * 8)]:
            add(['T', direction, 0], payload)
        for size in range(12):
            add(['T', direction, 1], rng.randbytes(size))
        for ident in [-1, 2, 55]:
            add(['T', direction, ident], rng.randbytes(23))
    for _ in range(500):
        add(['H', 0], rng.randbytes(rng.randrange(30)))
        add(['T', rng.randrange(2), rng.randrange(2)], rng.randbytes(rng.randrange(30)))
    for ident in [-1, 1, 55]:
        add(['H', ident], rng.randbytes(13))
    # Constructed palette wire output, including the reference's sparse fallback.
    for blocks, gb, count in [(1, 16, 4096), (0, 7, 64)]:
        for distinct in [1, 2, 8, 9, 16, 17, 200, 256, 257, 300]:
            values = [rng.randrange(distinct) for _ in range(count)]
            commands.append(f'V {blocks} {gb} ' + ' '.join(map(str, values)))
        sparse = [1000000 + i * 31 for i in range(count)]
        commands.append(f'V {blocks} 32 ' + ' '.join(map(str, sparse)))
        negative = [-1, 1000001, -1, 2000002, 1000001]
        commands.append(f'V {blocks} 32 ' + ' '.join(map(str, negative)))
        for leading in [0, 1, 2, 3, 4, 5, 8, 9, 15, 16, 17, 31, 32]:
            local = leading <= (8 if blocks else 3)
            actual = 0 if leading == 0 else max(4, leading) if blocks and local else leading if local else max(gb, leading)
            palette = [7] if leading == 0 else [7, 9]
            values = [i % 2 for i in range(count)]
            raw = encode_palette(bool(blocks), gb, leading, palette, values)
            for end in sorted({0, 1, 2, 3, 4, len(raw) // 2, max(0, len(raw) - 1), len(raw)}):
                add(['C', blocks, gb], raw[:end])
            add(['C', blocks, gb], raw + b'extra')
        for leading in [33, 200, 255]:
            add(['C', blocks, gb], bytes([leading]))
        add(['C', blocks, gb], b'\x04' + varint(-1))
    # Section data includes both signed counters and two differently configured palettes.
    for distinct in [1, 2, 17, 257]:
        leading = 0 if distinct == 1 else 4 if distinct <= 16 else 5 if distinct <= 32 else 16
        palette = list(range(distinct))
        states = encode_palette(True, 16, leading, palette, [i % distinct for i in range(4096)])
        biomes = encode_palette(False, 7, 1, [2, 3], [i % 2 for i in range(64)])
        for counter in [0, 256, -1, 32767]:
            raw = counter.to_bytes(2, 'big', signed=True) + b'\x00\x01' + states + biomes
            for end in sorted({*range(min(16, len(raw))), len(raw) // 2, len(raw) - 1, len(raw)}):
                add(['S', 16, 7], raw[:end])
            add(['S', 16, 7], raw + b'trailing')
    return ('\n'.join(commands) + '\n').encode(), len(commands)


def extract_test(source, name):
    start = source.index('    #[test]\n    fn ' + name + '(')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def reference_snapshot(root, reference, lock, out):
    names = ['lib.rs', 'buf.rs', 'chunk.rs', 'ids.rs', 'packets/mod.rs', 'packets/handshake.rs', 'packets/status.rs']
    source = {}
    for name in names:
        raw = (reference / (BASE + name)).read_bytes()
        if hashlib.sha256(raw).hexdigest() != lock['sources'][BASE + name]:
            raise SystemExit('Protocol reference hash mismatch: ' + name)
        source[name] = raw.decode()
    snapshot = out / 'protocol_reference'
    (snapshot / 'packets').mkdir(parents=True, exist_ok=True)
    buf = source['buf.rs'].replace('use horizon_world::nbt::{self, Tag};\n', '')
    for signature in ['    pub fn nbt(&mut self)', '    pub fn nbt(&mut self, tag: &Tag)']:
        start = buf.index(signature)
        end = buf.index('    pub fn long_array', start)
        buf = buf[:start] + buf[end:]
    start = buf.index('pub struct Nbt;')
    end = buf.index('/// ByteBufCodecs.LONG_ARRAY.', start)
    buf = buf[:start] + buf[end:]
    (snapshot / 'buf.rs').write_text(buf)
    chunk = source['chunk.rs'].replace('use horizon_world::nbt::Tag;\n', '')
    chunk = chunk[:chunk.index('packet!(\n    /// ClientboundLevelChunkPacketData.BlockEntityInfo.')]
    tests = ['from_values_matches_the_linear_search', 'bit_storage_matches_simple_bit_storage',
             'palettes_follow_the_strategies', 'wire_bits_wider_than_storage_are_rejected']
    chunk += '\n#[cfg(test)] mod tests {\nuse super::*;\n' + '\n'.join(extract_test(source['chunk.rs'], n) for n in tests) + '\n}\n'
    (snapshot / 'chunk.rs').write_text(chunk)
    packets = source['packets/mod.rs']
    for name in ['configuration', 'login', 'play', 'recipes']:
        packets = packets.replace(f'pub mod {name};\n', '')
    (snapshot / 'packets/mod.rs').write_text(packets)
    for name in ['handshake', 'status']:
        (snapshot / f'packets/{name}.rs').write_text(source[f'packets/{name}.rs'])
    template = (root / 'goose/tools/net_oracle.rs.in').read_text()
    template = template.replace('@LIB_TYPES@', source['lib.rs'][source['lib.rs'].index('/// Connection protocol states'):])
    for placeholder, path in {'@BUF_SOURCE@': snapshot / 'buf.rs', '@CHUNK_SOURCE@': snapshot / 'chunk.rs',
                              '@PACKETS_SOURCE@': snapshot / 'packets/mod.rs', '@IDS_SOURCE@': reference / (BASE + 'ids.rs')}.items():
        template = template.replace('"' + placeholder + '"', json.dumps(str(path)))
    (out / 'net_oracle.rs').write_text(template)
    return names


def check_net(args, reference, lock, out, run):
    root = Path(__file__).resolve().parents[2]
    names = reference_snapshot(root, reference, lock, out)
    print('Compiling original protocol tests (NBT/other states explicitly excluded)...', flush=True)
    run([args.rustc, '--edition=2024', '--test', '-O', out / 'net_oracle.rs', '-o', out / 'net_original_tests'])
    original_tests = run([out / 'net_original_tests']).decode()
    print(original_tests, end='')
    run([args.rustc, '--edition=2024', '-O', out / 'net_oracle.rs', '-o', out / 'net_oracle'])
    for suite in ['wire', 'chunk_wire', 'packets']:
        run([args.goose, 'build', '--debug', '-o', out / f'{suite}_translated_tests', root / f'goose/{suite}_test.goose'])
        print(run([out / f'{suite}_translated_tests']).decode(), end='')
    data, operations = fixtures()
    (out / 'net_fixtures.txt').write_bytes(data)
    expected = run([out / 'net_oracle'], input=data)
    (out / 'net_expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'net_{mode}', root / 'goose/net_probe.goose'])
        actual = run([out / f'net_{mode}'], input=data)
        (out / f'net_{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            mismatch = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            detail = f'\nRust: {b[mismatch][:300]!r}\nGoose: {a[mismatch][:300]!r}' if mismatch < min(len(a), len(b)) else ''
            raise SystemExit(f'Protocol {mode} divergence at fixture {mismatch + 1}; inspect transcripts in {out}' + detail)
    report = {'module': 'net/protocol-777-subset', 'minecraft': '26.3', 'revision': lock['revision'],
              'sources': {BASE + n: lock['sources'][BASE + n] for n in names},
              'fixture_operations': operations, 'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'], 'original_unit_tests': original_tests,
              'oracle_exclusions': ['buf.rs NBT import, Reader/Writer nbt methods and Nbt codec',
                                    'other packet states: configuration, login, play, recipes',
                                    'chunk.rs after Section (NBT block entities, outer chunk and lighting)'],
              'pending': ['float/velocity codecs', 'NBT and outer ChunkData/LightData', 'encryption/session integration and configuration/play codecs']}
    (out / 'net-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {operations} protocol fixtures; Rust/Goose cursor, errors, packet bodies and section bytes match in debug/release.')
    return report
