"""Pinned CFB8/MD5/offline UUID subset with an adapted OpenSSL AES backend."""
import hashlib
import json
from pathlib import Path
import random
import uuid


def check_crypto(args, reference, lock, out, run):
    root = Path(__file__).resolve().parents[2]
    name = 'crates/horizon-protocol/src/crypto.rs'
    raw = (reference / name).read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != lock['sources'][name]:
        raise SystemExit('Crypto reference differs from upstream.lock.json')
    text = raw.decode()
    # Explicit module boundaries; the retained Cfb8, md5, offline_uuid and
    # uuid_string definitions are unchanged. A read-only register probe is added.
    marks = ['/// AES/CFB8 stream state', "/// Fill `out`", '/// MD5 (RFC 1321).', '#[cfg(test)]']
    if any(text.count(m) != 1 for m in marks):
        raise SystemExit('Crypto boundary changed; refusing to infer a subset')
    subset = 'use crate::blocks::{Aes128, LANES};\n'
    subset += text[text.index(marks[0]):text.index(marks[1])]
    subset += text[text.index(marks[2]):text.index(marks[3])]
    subset += '\npub fn register(c: &Cfb8) -> &[u8;16] { &c.register }\n'
    work = out / 'crypto'
    work.mkdir(exist_ok=True)
    source = work / 'crypto-subset.rs'
    source.write_text(subset)
    adapter = work / 'blocks.rs'
    adapter.write_text((root / 'goose/tools/crypto_blocks_adapter.rs.in').read_text())
    template = (root / 'goose/tools/crypto_oracle.rs.in').read_text()
    template = template.replace('"@CRYPTO_SNAPSHOT@"', json.dumps(str(source)))
    template = template.replace('"@BLOCKS_ADAPTER@"', json.dumps(str(adapter)))
    (work / 'oracle.rs').write_text(template)
    cc = getattr(args, 'cc', 'cc')
    run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-c', root / 'goose/native/crypto.c', '-o', work / 'crypto.o'])
    run(['ar', 'rcs', work / 'libcrypto_adapter.a', work / 'crypto.o'])
    run([args.rustc, '--edition=2024', '-O', work / 'oracle.rs', '-L', work,
         '-l', 'static=crypto_adapter', '-l', 'crypto', '-o', work / 'oracle'])
    rng = random.Random(0x43525950544f)
    lines, independent = [], []
    def add(op, data):
        lines.append(op + ' ' + (data.hex() or '-'))
        if op == 'M':
            independent.append(hashlib.md5(data).hexdigest())
        else:
            try:
                data.decode('utf-8')
            except UnicodeError:
                independent.append('INVALID')
            else:
                independent.append(str(uuid.UUID(bytes=hashlib.md5(b'OfflinePlayer:' + data).digest(), version=3)))
    for length in [0, 1, 15, 16, 31, 32, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128, 129, 1024, 65536]:
        add('M', rng.randbytes(length))
    for _ in range(4000):
        add('M', rng.randbytes(rng.randrange(2048)))
    for s in ['', 'Xylar', 'Notch', 'a' * 16, '😀' * 8, '日本語', 'a\0b']:
        add('U', s.encode())
    for _ in range(4000):
        s = ''.join(chr(rng.randrange(0xd800)) for _ in range(rng.randrange(20)))
        add('U', s.encode())
    for data in [bytes([b]) for b in range(128, 256)] + [b'\xed\xa0\x80', b'\xc0\x80', b'\xf4\x90\x80\x80']:
        add('U', data)
    simple_count = len(lines)
    for length in [0, 1, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 512, 1024, 5000]:
        key, iv, data = rng.randbytes(16), rng.randbytes(16), rng.randbytes(length)
        for chunk in [1, 7, 16, 31, 32, 33, 64, 257, 65536]:
            for op in ['E', 'D']:
                lines.append(f'{op} {key.hex()} {iv.hex()} {chunk} ' + (data.hex() or '-'))
    data = ('\n'.join(lines) + '\n').encode()
    (work / 'fixtures.txt').write_bytes(data)
    expected = run([work / 'oracle'], input=data)
    if expected.decode().splitlines()[:simple_count] != independent:
        raise SystemExit('Original MD5/UUID definitions diverge from independent hashlib/uuid')
    (work / 'expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        exe = work / f'goose-{mode}'
        run([args.goose, 'build', *flags, '-o', exe, root / 'goose/crypto_probe.goose'])
        actual = run([exe], input=data)
        (work / f'{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            index = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            raise SystemExit(f'Crypto {mode} mismatch at fixture {index + 1}; inspect {work}')
    report = {'module': 'net/crypto-subset', 'revision': lock['revision'], 'source_sha256': digest,
              'fixture_lines': len(lines), 'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'],
              'oracle_adaptations': ['AES blocks and sequential CFB8 encryption dependency replaced with an OpenSSL adapter; Cfb8::decrypt, MD5 and UUID bodies retained verbatim.',
                                     'Read-only access to the private CFB8 register added for stream-state comparison.'],
              'independent_oracles': ['hashlib.md5 and uuid for all hash/UUID fixtures',
                                      'NIST CFB8 vector and OpenSSL CLI stream interoperability in the integration suite'],
              'excluded': ['random_bytes/generate_secret', 'java_hex_digest/server_hash', 'original unit tests requiring unavailable Cargo dependencies'],
              'pending': ['Microsoft session authentication', 'Rust RSA arithmetic (public operation is explicitly adapted to OpenSSL)']}
    (out / 'crypto-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(lines)} crypto fixtures; Rust/Goose MD5, UUID, CFB8 outputs and registers match in debug/release.')
    return report
