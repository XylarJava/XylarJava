"""Verify the scalar coordinate definitions, explicitly excluding iter.rs."""
import hashlib
import itertools
import json
import random
from pathlib import Path


def check_position(args, reference, lock, out, run):
    root = Path(__file__).resolve().parents[2]
    name = 'crates/horizon-core/src/position.rs'
    source = reference / name
    raw = source.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != lock['sources'][name]:
        raise SystemExit('Position source hash differs from the pinned reference')
    # Keep every scalar definition and original scalar test verbatim. The
    # iteration submodule depends on geometry and is a separate pending port;
    # no hand-written stand-in geometry is used in this oracle.
    exclusions = ['mod iter;\n', 'pub use iter::{BlockBoxIter, BlockIterationTooLarge};\n']
    snapshot = raw.decode()
    for line in exclusions:
        if snapshot.count(line) != 1:
            raise SystemExit('Position module boundary changed; refusing to infer an oracle subset')
        snapshot = snapshot.replace(line, '', 1)
    snapshot_path = out / 'position-scalar.rs'
    snapshot_path.write_text(snapshot)
    template = (root / 'goose/tools/position_oracle.rs.in').read_text()
    (out / 'position_oracle.rs').write_text(template.replace('"@POSITION_SNAPSHOT@"', json.dumps(str(snapshot_path))))
    run([args.rustc, '--edition=2024', '--test', '-O', out / 'position_oracle.rs', '-o', out / 'position_original_tests'])
    original_tests = run([out / 'position_original_tests']).decode()
    print(original_tests, end='')
    run([args.rustc, '--edition=2024', '-O', out / 'position_oracle.rs', '-o', out / 'position_oracle'])
    run([args.goose, 'build', '--debug', '-o', out / 'position_translated_tests', root / 'goose/position_test.goose'])
    print(run([out / 'position_translated_tests']).decode(), end='')
    rng = random.Random(0x504F534954494F4E)
    boundaries = [-2147483648, -33554433, -33554432, -2097153, -2097152,
                  -524289, -524288, -2049, -2048, -33, -32, -17, -16, -1,
                  0, 1, 15, 16, 31, 32, 2047, 2048, 524287, 2097151, 33554431, 2147483647]
    cases = []
    for x, y, z in itertools.product(boundaries, repeat=3):
        cases.append([x, y, z, rng.choice(boundaries), rng.choice(boundaries), rng.choice(boundaries),
                      rng.randrange(-(1 << 63), 1 << 63), rng.randrange(1 << 16)])
    for _ in range(20000):
        cases.append([*[rng.randrange(-(1 << 31), 1 << 31) for _ in range(6)],
                      rng.randrange(-(1 << 63), 1 << 63), rng.randrange(1 << 16)])
    data = ('\n'.join(' '.join(map(str, c)) for c in cases) + '\n').encode()
    (out / 'position_fixtures.txt').write_bytes(data)
    expected = run([out / 'position_oracle'], input=data)
    (out / 'position_expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'position_{mode}', root / 'goose/position_probe.goose'])
        actual = run([out / f'position_{mode}'], input=data)
        (out / f'position_{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            mismatch = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            raise SystemExit(f'Position {mode} divergence at fixture {mismatch + 1}; inspect transcripts in {out}')
    report = {'module': 'core/position-scalar', 'revision': lock['revision'], 'source_sha256': digest,
              'fixture_cases': len(cases), 'integer_fields_compared_per_case': 36,
              'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'], 'original_unit_tests': original_tests,
              'oracle_exclusions': exclusions, 'pending': ['position/iter.rs, geometry-dependent box iteration']}
    (out / 'position-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(cases)} coordinate fixtures, 36 fields each; Rust/Goose match in debug/release (scalar definitions only).')
    return report
