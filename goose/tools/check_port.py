#!/usr/bin/env python3
"""Compile pinned Rust definitions and compare them to native Goose.

No Cargo downloads, generated game data, player directories or Python gameplay.
Fixtures and full comparison output stay in the ignored build directory.
"""
import argparse
import hashlib
import json
import platform
import random
import shlex
import shutil
import subprocess
from pathlib import Path
from paletted_parity import check_paletted
from position_parity import check_position
from net_parity import check_net
from frame_parity import check_frame
from login_parity import check_login
from random_parity import check_random
from jmath_parity import check_jmath
from crypto_parity import check_crypto
from nbt_parity import check_nbt
from configuration_parity import check_configuration
from chunk_data_parity import check_chunk_data
from player_sync_parity import check_player_sync
from registry_parity import check_registry

ROOT = Path(__file__).resolve().parents[2]
SOURCE = 'crates/horizon-core/src/storage.rs'


def check_seed_hash(args, reference, lock, out):
    name = 'crates/horizon-core/src/biome/seed_hash.rs'
    source = reference / name
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if digest != lock['sources'][name]:
        raise SystemExit('Seed hash reference differs from upstream.lock.json')
    template = (ROOT / 'goose/tools/seed_hash_oracle.rs.in').read_text()
    (out / 'seed_hash_oracle.rs').write_text(template.replace('"@SEED_HASH_SOURCE@"', json.dumps(str(source))))
    run([args.rustc, '--edition=2024', '-O', out / 'seed_hash_oracle.rs', '-o', out / 'seed_hash_oracle'])
    run([args.goose, 'build', '--debug', '-o', out / 'seed_hash_translated_tests', ROOT / 'goose/seed_hash_test.goose'])
    print(run([out / 'seed_hash_translated_tests']).decode(), end='')
    rng = random.Random(0x53454544)
    seeds = [0, 1, -1, 42, -(1 << 63), (1 << 63) - 1]
    for bit in range(63):
        seeds.extend([1 << bit, -(1 << bit), (1 << bit) - 1, -((1 << bit) - 1)])
    seeds.extend(rng.randrange(-(1 << 63), 1 << 63) for _ in range(20000))
    data = ('\n'.join(map(str, seeds)) + '\n').encode()
    (out / 'seed_hash_fixtures.txt').write_bytes(data)
    expected = run([out / 'seed_hash_oracle'], input=data)
    independent = ('\n'.join(str(int.from_bytes(hashlib.sha256(s.to_bytes(8, 'little', signed=True)).digest()[:8],
                                               'little', signed=True)) for s in seeds) + '\n').encode()
    if expected != independent:
        raise SystemExit('Rust seed hash differs from independent hashlib SHA-256')
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'seed_hash_{mode}', ROOT / 'goose/seed_hash_probe.goose'])
        actual = run([out / f'seed_hash_{mode}'], input=data)
        (out / f'seed_hash_{mode}.txt').write_bytes(actual)
        if actual != expected:
            raise SystemExit(f'Seed hash {mode} output differs from Rust/hashlib')
    report = {'module': 'core/seed_hash', 'revision': lock['revision'], 'source_sha256': digest,
              'fixture_cases': len(seeds), 'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'], 'independent_oracle': 'hashlib.sha256'}
    (out / 'seed_hash-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(seeds)} world seeds, exact Rust/Goose/hashlib match in debug and release.')
    return report


def check_text(args, reference, lock, out):
    source = reference / 'crates/horizon-core/src/text.rs'
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if digest != lock['sources']['crates/horizon-core/src/text.rs']:
        raise SystemExit('Text reference differs from upstream.lock.json')
    template = (ROOT / 'goose/tools/text_oracle.rs.in').read_text()
    (out / 'text_oracle.rs').write_text(template.replace('"@TEXT_SOURCE@"', json.dumps(str(source))))
    run([args.rustc, '--edition=2024', '-O', out / 'text_oracle.rs', '-o', out / 'text_oracle'])
    run([args.goose, 'build', '--debug', '-o', out / 'text_translated_tests', ROOT / 'goose/text_test.goose'])
    print(run([out / 'text_translated_tests']).decode(), end='')
    rng = random.Random(0x54455854)
    cases = [b'', b'+', b'-', b'2147483647', b'-2147483648', b'2147483648',
             b'-2147483649', b' 1', b'1 ', b'++1', b'1\n', b'1\x00', b'0x10',
             b'0' * 10000 + b'2147483647', b'-' + b'0' * 10000 + b'2147483648']
    # Exhaust every BMP scalar, both alone and following an accepted digit.
    for cp in range(65536):
        if 0xd800 <= cp <= 0xdfff:
            continue
        value = chr(cp).encode()
        cases.extend([value, b'1' + value])
    for _ in range(3000):
        value = str(rng.randrange(-3000000000, 3000000001))
        # Mixed accepted BMP scripts, rather than an ASCII-only parser check.
        zeroes = [0x30, 0x660, 0x966, 0xff10]
        value = ''.join(chr(rng.choice(zeroes) + int(c)) if c.isdigit() else c for c in value)
        cases.append(value.encode())
    for cp in [0x10000, 0x1d7ce, 0x1d7d9, 0x10ffff, 0x1f600]:
        cases.append(chr(cp).encode())
    # Rust str cannot carry malformed UTF-8; both probe boundaries reject it.
    cases.extend([bytes([b]) for b in range(128, 256)])
    cases.extend([b'\xc0\xb1', b'\xc1\xb1', b'\xe0\x80\xb1', b'\xed\xa0\x80',
                  b'\xe1\x31\xb1', b'\xf4\x90\x80\x80', b'\xe0\xa0', b'\xc2'])
    data = ('\n'.join(case.hex() for case in cases) + '\n').encode()
    (out / 'text_fixtures.txt').write_bytes(data)
    expected = run([out / 'text_oracle'], input=data)
    (out / 'text_expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'text_{mode}', ROOT / 'goose/text_probe.goose'])
        actual = run([out / f'text_{mode}'], input=data)
        (out / f'text_{mode}.txt').write_bytes(actual)
        if actual != expected:
            raise SystemExit(f'Text {mode} output diverges; inspect files in {out}')
    report = {'module': 'core/text', 'revision': lock['revision'], 'source_sha256': digest,
              'fixture_cases': len(cases), 'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release']}
    (out / 'text-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(cases)} text fixtures, exact Rust/Goose match in debug and release.')
    return report


def run(command, **kwargs):
    command = [str(c) for c in command]
    try:
        result = subprocess.run(command, check=True, timeout=60,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs)
    except subprocess.CalledProcessError as error:
        raise SystemExit(f'Command failed: {shlex.join(command)}\n'
                         + error.stderr.decode(errors='replace')
                         + error.stdout.decode(errors='replace')) from error
    return result.stdout


def fixtures():
    rng = random.Random(0x58594C4152)
    lines = []
    for bits in [0, 33, 255]:
        lines += [f'N {bits} 1 1', 'G 0']
    for bits in range(1, 33):
        per = 64 // bits
        mask = (1 << bits) - 1
        for length in sorted({0, 1, per - 1, per, per + 1, 256, 4096, 16385}):
            lines += [f'N {bits} {length} 1', 'D', f'G {length}',
                      f'S {length} 4294967295', 'D']
            for batch in range(3):
                lines += [f'A {batch}', 'D']
            if length:
                valid = min(mask, 0x7fffffff)
                lines += [f'S 0 {valid}', 'G 0', f'G {length - 1}',
                          f'S 0 {min(0xffffffff, mask + 1)}', 'D',
                          f'L {valid}', 'D', 'L 4294967295', 'D']
        # Force the portable division strategy on ordinary section sizes too.
        lines += [f'N {bits} 4096 0', 'A 13', 'D']
        for length in [1, per, per + 1]:
            size = (length + per - 1) // per
            words = [rng.randrange(-(1 << 63), 1 << 63) for _ in range(size)]
            lines += [f'F {bits} {length} 1 ' + ' '.join(map(str, words)), 'D',
                      'S 0 0', 'D', 'L 1', 'D']
        # Length errors, including empty/extra buffers and invalid-bit priority.
        lines += [f'F {bits} {per + 1} 1 0', f'F {bits} 0 1 0', 'D']
    lines += ['F 0 13 1 0', 'F 5 1 1 -1', 'S 0 0', 'S 0 32', 'D',
              'F 32 1 1 4294967295', 'G 0', 'S 0 4294967295', 'D',
              'W 0 4294967294', 'G 0', 'D']
    # Reproducible mixed updates, covering all error payloads and atomic writes.
    for bits in range(1, 33):
        lines.append(f'N {bits} 97 1')
        for _ in range(80):
            index = rng.randrange(110)
            value = rng.randrange(1 << min(bits + 1, 32))
            lines += [f'S {index} {value}', f'G {index}']
        lines.append('D')
    return ('\n'.join(lines) + '\n').encode(), len(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path, default=ROOT / '.cache/horizon-upstream')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-goose/port')
    parser.add_argument('--goose', default=shutil.which('goose'))
    parser.add_argument('--rustc', default=shutil.which('rustc'))
    args = parser.parse_args()
    if not args.goose or not args.rustc:
        parser.error('the aardappel Goose compiler and rustc are required')
    lock = json.loads((ROOT / 'goose/upstream.lock.json').read_text())
    reference = args.upstream.resolve()
    revision = run(['git', '-C', reference, 'rev-parse', 'HEAD']).decode().strip()
    source = reference / SOURCE
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if revision != lock['revision'] or digest != lock['sources'][SOURCE]:
        raise SystemExit('Reference revision/source differs from upstream.lock.json; refusing an unpinned comparison')
    # The ledger also records integration sources whose full Rust dependency
    # graph is not compiled by an oracle. Verify provenance for every entry.
    for name, expected in lock['sources'].items():
        if hashlib.sha256((reference / name).read_bytes()).hexdigest() != expected:
            raise SystemExit(f'Reference source differs from upstream.lock.json: {name}')
    out = args.build_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    template = (ROOT / 'goose/tools/storage_oracle.rs.in').read_text()
    # JSON string syntax is valid for this absolute UTF-8 Rust path literal.
    oracle_source = template.replace('"@STORAGE_SOURCE@"', json.dumps(str(source)))
    (out / 'storage_oracle.rs').write_text(oracle_source)
    print('Compiling and running the original Rust storage unit tests...', flush=True)
    run([args.rustc, '--edition=2024', '--test', '-C', 'overflow-checks=yes',
         out / 'storage_oracle.rs', '-o', out / 'storage_original_tests'])
    original_tests = run([out / 'storage_original_tests']).decode()
    print(original_tests, end='')
    run([args.rustc, '--edition=2024', '-O', out / 'storage_oracle.rs', '-o', out / 'storage_oracle'])
    print('Compiling debug/optimized Goose probes and the translated tests...', flush=True)
    run([args.goose, 'build', '--debug', '-o', out / 'storage_translated_tests', ROOT / 'goose/storage_test.goose'])
    translated_tests = run([out / 'storage_translated_tests']).decode()
    print(translated_tests, end='')
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'storage_{mode}', ROOT / 'goose/storage_probe.goose'])
    data, operations = fixtures()
    (out / 'storage_fixtures.txt').write_bytes(data)
    expected = run([out / 'storage_oracle'], input=data)
    (out / 'storage_expected.txt').write_bytes(expected)
    for mode in ['debug', 'release']:
        actual = run([out / f'storage_{mode}'], input=data)
        (out / f'storage_{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            mismatch = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            raise SystemExit(f'{mode} diverges at output line {mismatch + 1}; inspect comparison files in {out}')
    observations = []
    if platform.machine().lower() in ['x86_64', 'amd64', 'i386', 'i686']:
        # The source's advertised MAGIC_LEN bound is too wide for u32 products
        # at some widths. Preserve and explicitly report this source behavior.
        edge = b'N 31 16384 1\nW 0 17\nW 8192 29\nG 0\nG 8192\n'
        baseline = run([out / 'storage_oracle'], input=edge)
        for mode in ['debug', 'release']:
            if run([out / f'storage_{mode}'], input=edge) != baseline:
                raise SystemExit(f'{mode}: large-index source edge case diverged')
        if baseline.splitlines()[-2:] == [b'OK 29', b'OK 29']:
            observations.append('Reference x86 multiply-shift aliases indices 0 and 8192 at bits=31,len=16384; reproduced, not silently fixed. Sizes 4096 and >16384 do not use this failing domain.')
    report = {
        'module': 'core/storage', 'revision': revision, 'source_sha256': digest,
        'fixture_operations': operations, 'output_lines_compared_per_build': len(expected.splitlines()),
        'output_sha256': hashlib.sha256(expected).hexdigest(),
        'matched_builds': ['goose-debug', 'goose-release'],
        'original_unit_tests': original_tests, 'translated_unit_tests': translated_tests,
        'reference_observations': observations,
        'goose_version': run([args.goose, 'version']).decode().strip(),
        'rustc_version': run([args.rustc, '--version']).decode().strip(),
    }
    (out / 'storage-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {operations} fixture operations, exact Rust/Goose output match in debug and release.')
    for observation in observations:
        print('Reference observation:', observation)
    print('Report:', out / 'storage-report.json')
    text_report = check_text(args, reference, lock, out)
    seed_report = check_seed_hash(args, reference, lock, out)
    palette_report = check_paletted(args, reference, lock, out, run)
    position_report = check_position(args, reference, lock, out, run)
    net_report = check_net(args, reference, lock, out, run)
    frame_report = check_frame(args, reference, lock, out, run)
    login_report = check_login(args, reference, lock, out, run)
    jmath_report = check_jmath(args, reference, lock, out, run)
    random_report = check_random(args, reference, lock, out, run)
    crypto_report = check_crypto(args, reference, lock, out, run)
    nbt_report = check_nbt(args, reference, lock, out, run)
    configuration_report = check_configuration(args, reference, lock, out, run)
    chunk_data_report = check_chunk_data(args, reference, lock, out, run)
    player_sync_report = check_player_sync(args, reference, lock, out, run)
    registry_report = check_registry(args, reference, lock, out, run)
    (out / 'port-report.json').write_text(json.dumps({'modules': [report, text_report, seed_report, palette_report, position_report, net_report, frame_report, login_report, jmath_report, random_report, crypto_report, nbt_report, configuration_report, chunk_data_report, player_sync_report, registry_report]}, indent=2) + '\n')


if __name__ == '__main__':
    main()
