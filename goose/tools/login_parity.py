"""Compare all 26.3 login packet fields, canonical encodings and errors."""
import hashlib
import json
import random
from pathlib import Path
from net_parity import BASE, reference_snapshot, string, varint


def fixtures():
    rng = random.Random(0x4c4f47494e373737)
    commands = []
    packets = []
    uuid = bytes(range(16))
    session = b'\xff' * 16

    def add(direction, ident, data):
        commands.append(f'{direction} {ident} ' + (data.hex() or '-'))

    def prop(name='textures', value='e30=', signature=None):
        return string(name) + string(value) + (b'\0' if signature is None else b'\x01' + string(signature))

    def profile(name, properties):
        return uuid + string(name) + varint(len(properties)) + b''.join(properties) + session

    for name in ['', 'Xylar', 'a' * 16, 'a' * 17, '😀' * 8, '😀' * 9, b'\xff']:
        packets.append(('S', 0, string(name) + uuid))
        for properties in [[], [prop()], [prop(), prop('signed', 'v', 'sig')]]:
            packets.append(('C', 2, profile(name, properties)))
    for name in ['a' * 64, 'a' * 65, '😀' * 32, '😀' * 33]:
        packets.append(('C', 2, profile('Xylar', [prop(name)])))
    for value in ['a' * 32767, 'a' * 32768, '😀' * 16383, '😀' * 16384]:
        packets.append(('C', 2, profile('Xylar', [prop(value=value)])))
    for signature in ['', 's' * 1024, 's' * 1025, '😀' * 512, '😀' * 513]:
        packets.append(('C', 2, profile('Xylar', [prop(signature=signature)])))
    for reason in ['', '{"text":"bye"}', 'a' * 262144, 'a' * 262145, b'\xed\xa0\x80']:
        packets.append(('C', 0, string(reason)))
    for server_id in ['', 'id', 'a' * 20, 'a' * 21, '😀' * 10, '😀' * 11]:
        for flag in [0, 1, 255]:
            packets.append(('C', 1, string(server_id) + string(b'\x01\x02') + string(b'\x03' * 4) + bytes([flag])))
    for n in [0, 1, 127, 128, 256, -1, -2147483648, 2147483647]:
        packets.append(('C', 3, varint(n)))
        for channel in ['', 'velocity:player_info', 'x', b'\xff']:
            packets.append(('C', 4, varint(n) + string(channel) + b'\x01\xff'))
    for key in ['', 'a:b', 'x', 'a' * 32767, 'a' * 32768, b'\xff']:
        packets.append(('C', 5, string(key)))
        for flag in [0, 1, 255]:
            packets.append(('S', 4, string(key) + bytes([flag]) + (string(b'cookie') if flag else b'')))
    for length in [0, 1, 16, 128, 1024]:
        packets.append(('S', 1, string(b'\x09' * length) + string(b'\x08' * length)))
    for transaction in [0, 3, -1, 2147483647]:
        for flag in [0, 1, 2, 255]:
            for data in [b'', b'\x00', b'payload']:
                packets.append(('S', 2, varint(transaction) + bytes([flag]) + data))
    packets.append(('S', 3, b''))
    for direction, ident, data in packets:
        add(direction, ident, data)
        add(direction, ident, data + b'\x00')
        for size in sorted({0, 1, 2, 3, 4, 15, 16, 17, len(data) // 2, max(0, len(data) - 1)}):
            if size < len(data):
                add(direction, ident, data[:size])
    for ident in [-1, 6, 99, 2147483647]:
        add('C', ident, rng.randbytes(61))
        add('S', ident, rng.randbytes(61))
    for direction, ids in [('C', range(6)), ('S', range(5))]:
        for ident in ids:
            for _ in range(500):
                add(direction, ident, rng.randbytes(rng.randrange(128)))
    # Count/length failure order for nested lists and byte arrays.
    for n in [-1, 1, 2, 10000, 2147483647]:
        for trailing in [b'', b'\0', prop()]:
            add('C', 2, uuid + string('Xylar') + varint(n) + trailing)
            add('C', 1, string('') + varint(n) + trailing)
            add('S', 1, varint(n) + trailing)
    return ('\n'.join(commands) + '\n').encode(), len(commands)


def check_login(args, reference, lock, out, run):
    root = Path(__file__).resolve().parents[2]
    source = BASE + 'packets/login.rs'
    raw = (reference / source).read_bytes()
    sha = hashlib.sha256(raw).hexdigest()
    if sha != lock['sources'][source]:
        raise SystemExit('Login reference hash mismatch')
    work = out / 'login'
    work.mkdir(exist_ok=True)
    reference_snapshot(root, reference, lock, work)
    snapshot = work / 'protocol_reference'
    p = snapshot / 'packets/mod.rs'
    p.write_text(p.read_text().replace('pub mod status;\n', 'pub mod status;\npub mod login;\n'))
    # The original round-trip test only needs a UUID value. Keep codec bodies
    # verbatim; substitute its unported crypto helper with a fixed test UUID.
    login = raw.decode().replace('crate::crypto::offline_uuid("Horizon")', '0x0123456789abcdef_fedcba9876543210u128')
    (snapshot / 'packets/login.rs').write_text(login)
    lib = (reference / (BASE + 'lib.rs')).read_text()
    template = (root / 'goose/tools/login_oracle.rs.in').read_text()
    template = template.replace('@LIB_TYPES@', lib[lib.index('/// Connection protocol states'):])
    for placeholder, path in {'@BUF_SOURCE@': snapshot / 'buf.rs', '@IDS_SOURCE@': reference / (BASE + 'ids.rs'),
                              '@PACKETS_SOURCE@': snapshot / 'packets/mod.rs'}.items():
        template = template.replace('"' + placeholder + '"', json.dumps(str(path)))
    (work / 'login_oracle.rs').write_text(template)
    run([args.rustc, '--edition=2024', '--test', '-O', work / 'login_oracle.rs', '-o', work / 'original_tests'])
    tests = run([work / 'original_tests', 'packets::login::tests::']).decode()
    print(tests, end='')
    run([args.rustc, '--edition=2024', '-O', work / 'login_oracle.rs', '-o', work / 'oracle'])
    data, count = fixtures()
    (work / 'fixtures.txt').write_bytes(data)
    expected = run([work / 'oracle'], input=data)
    (work / 'expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        executable = work / f'goose-{mode}'
        run([args.goose, 'build', *flags, '-o', executable, root / 'goose/login_probe.goose'])
        actual = run([executable], input=data)
        (work / f'goose-{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            mismatch = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            detail = f'\nGoose: {a[mismatch][:300]!r}\nRust: {b[mismatch][:300]!r}' if mismatch < min(len(a), len(b)) else ''
            raise SystemExit(f'Login {mode} divergence at fixture {mismatch + 1}' + detail)
    report = {'module': 'net/login', 'minecraft': '26.3', 'revision': lock['revision'], 'source_sha256': sha,
              'fixture_operations': count, 'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'], 'original_login_tests': tests,
              'scope': 'All six clientbound and five serverbound login packet codecs, profile/property nesting and unknown packets.',
              'oracle_adaptations': ['Original codec definitions and macros retained.',
                                     'One offline_uuid call in the original round-trip test fixture is replaced by a fixed u128; crypto is not tested.',
                                     'NBT buffer methods and configuration/play/recipes packet modules remain excluded.'],
              'pending': ['MSA session join and configuration/play state machine'],
              'integration_evidence': 'session-{debug,release}-report.json (separate stream integration, not codec parity)'}
    (out / 'login-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {count} login fixtures; exact fields, canonical encodings and errors in Rust/Goose debug/release.')
    return report
