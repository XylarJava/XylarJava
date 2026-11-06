"""Compare original frame policy, Goose, and Python's zlib framing oracle.

The unavailable Cargo flate2/zlib-rs dependency is explicitly substituted by
a test-only adapter to system zlib. This checks original framing policy, not
upstream zlib-rs's compressed output choices or backend diagnostic text.
"""
import hashlib
import json
import random
import zlib
from pathlib import Path
from net_parity import reference_snapshot, varint

MAX_FRAME = (1 << 21) - 1
MAX_UNCOMPRESSED = 8388608


def read_varint(data):
    value = 0
    for i in range(5):
        if i >= len(data):
            return None, None, 'needed 1 bytes, 0 left'
        byte = data[i]
        value = (value | ((byte & 127) << (7 * i))) & 0xffffffff
        if byte < 128:
            return value - (1 << 32) if value >= (1 << 31) else value, i + 1, None
    return None, None, 'VarInt too big'


def header(data):
    length = 0
    for i in range(3):
        if i >= len(data):
            return 'NONE'
        length |= (data[i] & 127) << (7 * i)
        if data[i] < 128:
            return f'HEADER {i + 1} {length}' if length else 'ERR Frame length cannot be zero'
    return 'ERR length wider than 21 bits'


def encode(data, threshold):
    if threshold < 0:
        inner = data
    elif len(data) < threshold:
        inner = b'\0' + data
    else:
        inner = varint(len(data)) + zlib.compress(data)
    if len(inner) > MAX_FRAME:
        return None, f'packet too large: {len(inner)} bytes'
    return varint(len(inner)) + inner, None


def body(data, threshold):
    if threshold < 0:
        return 'PLAIN', 0, data, None
    size, start, error = read_varint(data)
    if error:
        return None, None, None, error
    if size == 0:
        return 'PLAIN', start, data[start:], None
    if size < 0:
        return None, None, None, 'negative uncompressed length'
    if size < threshold:
        return None, None, None, f'Badly compressed packet - size of {size} is below server threshold of {threshold}'
    if size > MAX_UNCOMPRESSED:
        return None, None, None, f'Badly compressed packet - size of {size} is larger than protocol maximum'
    inflater = zlib.decompressobj()
    try:
        output = inflater.decompress(data[start:], size + 1)
    except zlib.error as e:
        code = 2 if 'Error 2 ' in str(e) else -3
        return None, None, None, f'Badly compressed packet - zlib status {code}'
    if len(output) != size or not inflater.eof:
        return None, None, None, f'Badly compressed packet - actual length {len(output)} does not match declared size {size}'
    return 'INFLATED', None, output, None


def digest(data):
    h = 0xcbf29ce484222325
    for byte in data:
        h = ((h ^ byte) * 0x100000001b3) & ((1 << 64) - 1)
    return h


def expected_line(line):
    p = line.split()
    raw = lambda s: bytes.fromhex(s) if s != '-' else b''
    if p[0] == 'H':
        return header(raw(p[1]))
    if p[0] == 'P':
        data = raw(p[1])
        return 'PREFIX ' + (varint(len(data)) + data).hex()
    threshold = int(p[1])
    if p[0] == 'E':
        output, error = encode(raw(p[2]), threshold)
        return 'ERR ' + error if error else 'ENC ' + output.hex()
    if p[0] == 'B':
        kind, at, output, error = body(raw(p[2]), threshold)
        if error:
            return 'ERR ' + error
        return f'PLAIN {at} ' + output.hex() if kind == 'PLAIN' else 'INFLATED ' + output.hex()
    if p[0] == 'X':
        n, kind = map(int, p[2:4])
        data = bytes(i % 7 if kind == 0 else ((i * 0x9e3779b97f4a7c15) & ((1 << 64) - 1)) >> 56 for i in range(n))
        output, error = encode(data, threshold)
        if error:
            return 'ERR ' + error
        _, at, length = header(output).split()
        _, _, result, error = body(output[int(at):int(at) + int(length)], threshold)
        if error:
            return f'BODYERR {len(output)} {digest(output)} {error}'
        return f'LARGE {len(output)} {digest(output)} {digest(result)}'
    raise AssertionError('Unknown framing fixture')


def fixtures():
    rng = random.Random(0x4652414d45373737)
    commands = []

    def add(op, data, threshold=None):
        commands.append(' '.join([op, *([] if threshold is None else [str(threshold)]), data.hex() or '-']))

    add('H', b'')
    for byte in range(256):
        add('H', bytes([byte]))
    for number in range(65536):
        add('H', number.to_bytes(2, 'big'))
    for _ in range(8192):
        add('H', rng.randbytes(3))
    for n in [0, 1, 127, 128, 16383, 16384, MAX_FRAME, MAX_FRAME + 1, -1]:
        add('H', varint(n))
    for data in [b'\x80\x80\x80', b'\x80\x00', b'\xff\xff\x7f', b'\x81\x80\x00', b'\x01\xff']:
        add('H', data)
    for length in [0, 1, 2, 10, 127, 128, 255, 256, 257, 1024, 5000, 70000]:
        data = bytes(i % 13 for i in range(length))
        add('P', data)
        for threshold in [-1, 0, 1, 256, 100000]:
            add('E', data, threshold)
    # A compressed packet can exceed the frame quota even when compression is enabled.
    add('E', rng.randbytes(MAX_FRAME + 1), 0)
    for _ in range(300):
        add('E', rng.randbytes(rng.randrange(4096)), rng.choice([-1, 0, 128, 256, 4096]))
    for size in [-1, -2147483648, 0, 1, 255, 256, 300, MAX_UNCOMPRESSED, MAX_UNCOMPRESSED + 1]:
        for threshold in [-1, 0, 256, MAX_UNCOMPRESSED + 2]:
            add('B', varint(size) + b'garbage', threshold)
    for raw in [b'', b'\x80', b'\xff' * 5, b'\x80\x00abc', b'\x80\x80\x80\x80\x00abc']:
        for threshold in [-1, 0, 256]:
            add('B', raw, threshold)
    for n in [1, 10, 256, 300, 5000, 70000]:
        data = bytes(i % 7 for i in range(n))
        encoded = zlib.compress(data)
        for declared in [n - 1, n, n + 1]:
            add('B', varint(declared) + encoded, 0)
        # Stream resets must recover after short/long declarations and damaged streams.
        for i in sorted({0, 1, 2, 3, len(encoded) // 2, len(encoded) - 1, len(encoded)}):
            add('B', varint(n) + encoded[:i], 0)
            add('B', varint(n) + encoded, 0)
        for index in [0, 1, len(encoded) // 2, len(encoded) - 1]:
            damaged = bytearray(encoded)
            damaged[index] ^= 255
            add('B', varint(n) + damaged, 0)
            add('B', varint(n) + encoded, 0)
        add('B', varint(n) + encoded + b'extra', 0)
        # Widen the uncompressed length VarInt without changing the value.
        wide = bytearray(varint(n))
        wide[-1] |= 128
        wide.append(0)
        add('B', wide + encoded, 0)
    compressor = zlib.compressobj(zdict=b'hello dictionary')
    add('B', varint(16) + compressor.compress(b'hello dictionary') + compressor.flush(), 0)
    for _ in range(150):
        add('B', varint(300) + rng.randbytes(rng.randrange(100)), 0)
    for threshold, length, kind in [(-1, MAX_FRAME, 1), (-1, MAX_FRAME + 1, 0),
                                     (MAX_FRAME + 2, MAX_FRAME, 0), (0, MAX_FRAME + 1, 1),
                                     (256, MAX_UNCOMPRESSED, 0), (256, MAX_UNCOMPRESSED + 1, 0)]:
        commands.append(f'X {threshold} {length} {kind}')
    return commands


def check_frame(args, reference, lock, out, run):
    root = Path(__file__).resolve().parents[2]
    source = 'crates/horizon-protocol/src/frame.rs'
    raw = (reference / source).read_bytes()
    sha = hashlib.sha256(raw).hexdigest()
    if sha != lock['sources'][source]:
        raise SystemExit('Frame reference hash mismatch')
    work = out / 'frames'
    work.mkdir(exist_ok=True)
    reference_snapshot(root, reference, lock, work)
    lib = (reference / 'crates/horizon-protocol/src/lib.rs').read_text()
    template = (root / 'goose/tools/frame_oracle.rs.in').read_text()
    template = template.replace('@LIB_TYPES@', lib[lib.index('/// Connection protocol states'):])
    template = template.replace('@ZLIB_ADAPTER@', (root / 'goose/tools/frame_zlib_adapter.rs.in').read_text())
    template = template.replace('"@BUF_SOURCE@"', json.dumps(str(work / 'protocol_reference/buf.rs')))
    template = template.replace('"@FRAME_SOURCE@"', json.dumps(str(reference / source)))
    (work / 'frame_oracle.rs').write_text(template)
    bridge = root / 'goose/native/compression.c'
    run(['cc', '-O2', '-Wall', '-Wextra', '-Werror', '-c', bridge, '-o', work / 'compression.o'])
    link = ['-C', f'link-arg={work / "compression.o"}', '-C', 'link-arg=-lz']
    run([args.rustc, '--edition=2024', '--test', '-O', work / 'frame_oracle.rs', *link, '-o', work / 'frame_original_tests'])
    original_tests = run([work / 'frame_original_tests', 'frame::tests::']).decode()
    print(original_tests, end='')
    run([args.rustc, '--edition=2024', '-O', work / 'frame_oracle.rs', *link, '-o', work / 'frame_oracle'])
    commands = fixtures()
    data = ('\n'.join(commands) + '\n').encode()
    (work / 'fixtures.txt').write_bytes(data)
    expected = ('\n'.join(expected_line(line) for line in commands) + '\n').encode()
    (work / 'expected.txt').write_bytes(expected)
    for mode, flags in [('rust-policy', None), ('goose-debug', ['--debug']), ('goose-release', [])]:
        if flags is None:
            executable = work / 'frame_oracle'
        else:
            executable = work / mode
            run([args.goose, 'build', *flags, '-o', executable, root / 'goose/frame_probe.goose'])
        actual = run([executable], input=data)
        (work / f'{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            mismatch = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            detail = f'\n{mode}: {a[mismatch][:300]!r}\nPython: {b[mismatch][:300]!r}' if mismatch < min(len(a), len(b)) else ''
            raise SystemExit(f'Framing divergence at fixture {mismatch + 1}: {commands[mismatch][:160]}' + detail)
    report = {'module': 'net/frame', 'minecraft': '26.3', 'revision': lock['revision'], 'source_sha256': sha,
              'fixture_operations': len(commands), 'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['rust-policy-system-zlib', 'goose-debug', 'goose-release'],
              'original_frame_tests': original_tests, 'zlib_runtime': zlib.ZLIB_RUNTIME_VERSION,
              'scope': 'Original frame.rs policy and five original tests; test-only flate2 dependency adapter uses system zlib.',
              'adaptations': ['System zlib replaces upstream flate2/zlib-rs; backend diagnostic strings use numeric zlib status.',
                              'Compressed byte choices of the upstream zlib-rs backend are not compared.',
                              'A separate Python framing implementation validates output and malformed inputs using Python zlib.',
                              'Goose inflater handles have exclusive ownership and explicit close; plain bodies borrow via offset.'],
              'pending': ['upstream zlib-rs cross-backend interoperability run', 'connection transport and login integration']}
    (out / 'frame-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(commands)} framing fixtures; Rust policy, Python/zlib and Goose debug/release match.')
    return report
