"""Compare seeded integer/float streams, cached Gaussians and triangles.

Only positional/worldgen/hash definitions are excluded from the pinned random
source. Gaussian draws use the unmodified pinned jmath snapshot prepared by
jmath_parity.py (only unexercised external dependency calls are substituted).
"""
import hashlib
import json
import random
from pathlib import Path
from jmath_parity import prepare_jmath

# Every excluded definition belongs to the unported positional/worldgen/hash
# boundary. Retained random draws and Gaussian bodies remain verbatim.
EXCLUSIONS = [
    ('mod hash;\n', 1),
    ('mod positional;\n', 1),
    ('mod worldgen;\n', 1),
    ('pub use hash::{seed_from_hash, seed_from_hash_utf16};\n', 1),
    ('pub use positional::{\n'
     '    LegacyPositionalRandom, PositionalRandom, XoroshiroPositionalRandom, java_string_hash,\n'
     '    position_seed,\n'
     '};\n', 1),
    ('pub use worldgen::{WorldgenPositional, WorldgenRandom, WorldgenSource, slime_chunk_random};\n', 1),
    ('    type Positional: PositionalRandom<Source = Self>;\n', 1),
    ('    fn fork_positional(&mut self) -> Self::Positional;\n', 1),
    ('    type Positional = LegacyPositionalRandom;\n'
     '    fn fork_positional(&mut self) -> Self::Positional {\n'
     '        LegacyPositionalRandom::new(self.next_long())\n'
     '    }\n', 1),
    ('    type Positional = XoroshiroPositionalRandom;\n'
     '    fn fork_positional(&mut self) -> Self::Positional {\n'
     '        let lo = self.next_long() as u64;\n'
     '        let hi = self.next_long() as u64;\n'
     '        XoroshiroPositionalRandom::new(lo, hi)\n'
     '    }\n', 1),
]

# A battery exercising every op, including invalid bounds, state, fork and the
# modulo-rejection path (bounds that are not powers of two).
BATTERY = [
    'i', 'i', 'i', 'l', 'l', 'b', 'b', 'b', 's',
    'B:1', 'B:2', 'B:3', 'B:7', 'B:16', 'B:17', 'B:100', 'B:255', 'B:256',
    'B:1000', 'B:46341', 'B:1431655765', 'B:2147483647',
    'B:0', 'B:-1', 'B:-2147483648',
    'R:-5:5', 'R:0:100', 'R:-100:-10', 'R:-2147483648:2147483647', 'R:5:5', 'R:10:3',
    'N:-10:10', 'N:0:0', 'N:7:7', 'N:-2147483648:2147483647',
    'C:1', 'i', 'C:5', 'l', 'F', 'i', 'F', 'l', 's',
]
SHORT = ['l', 'i', 'B:2', 'B:97', 'B:1000000000', 'R:-3:3', 'N:-1:1', 'b', 'l', 'F', 'i']


def check_random(args, reference, lock, out, run):
    root = Path(__file__).resolve().parents[2]
    name = 'crates/horizon-core/src/random.rs'
    source = reference / name
    raw = source.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != lock['sources'][name]:
        raise SystemExit('Random source hash differs from the pinned reference')
    snapshot = raw.decode()
    for text, expected in EXCLUSIONS:
        if snapshot.count(text) != expected:
            raise SystemExit('Random module boundary changed; refusing to infer an oracle subset')
        snapshot = snapshot.replace(text, '')
    jmath_digest, jmath_snapshot = prepare_jmath(reference, lock, out)
    snapshot_path = out / 'random-subset.rs'
    snapshot_path.write_text(snapshot)
    template = (root / 'goose/tools/random_oracle.rs.in').read_text()
    (out / 'random_oracle.rs').write_text(
        template.replace('"@RANDOM_SNAPSHOT@"', json.dumps(str(snapshot_path))))
    # Compile the jmath dependency without cfg(test); its unported platform
    # tests require Cargo libm, while the random module's original tests run.
    run([args.rustc, '--edition=2024', '-O', '--crate-type=rlib', '--crate-name=jmath',
         jmath_snapshot, '-o', out / 'libjmath.rlib'])
    dependency = ['--extern', f'jmath={out / "libjmath.rlib"}']
    run([args.rustc, '--edition=2024', *dependency, '--test', '-C', 'overflow-checks=yes',
         out / 'random_oracle.rs', '-o', out / 'random_original_tests'])
    original_tests = run([out / 'random_original_tests']).decode()
    print(original_tests, end='')
    run([args.rustc, '--edition=2024', *dependency, '-O', out / 'random_oracle.rs', '-o', out / 'random_oracle'])
    run([args.goose, 'build', '--debug', '-o', out / 'random_translated_tests', root / 'goose/random_test.goose'])
    print(run([out / 'random_translated_tests']).decode(), end='')

    rng = random.Random(0x52414E444F4D)
    seeds = [0, 1, -1, 2, -2, 42, -42, 1000, -1000,
             2147483647, -2147483648, 2147483648, -2147483649,
             9223372036854775807, -9223372036854775808]
    # Signed i64 bit patterns; i64::MIN/MAX are already in the base list above.
    for bit in range(63):
        seeds.extend([1 << bit, -(1 << bit)])
    lines = []
    for seed in seeds:
        lines.append('L ' + str(seed) + ' ' + ' '.join(BATTERY))
        lines.append('X ' + str(seed) + ' ' + ' '.join(BATTERY))
    for _ in range(8000):
        seed = rng.randrange(-(1 << 63), 1 << 63)
        lines.append('L ' + str(seed) + ' ' + ' '.join(SHORT))
        lines.append('X ' + str(seed) + ' ' + ' '.join(SHORT))
    # Random origin/bound and min/max pairs stress wrapping spans and rejection.
    for _ in range(4000):
        seed = rng.randrange(-(1 << 63), 1 << 63)
        o = rng.randrange(-(1 << 31), 1 << 31)
        b = rng.randrange(-(1 << 31), 1 << 31)
        bound = rng.randrange(-5, 1 << 31)
        ops = [f'R:{o}:{b}', f'N:{o}:{b}', f'B:{bound}', 'l', 'i']
        lines.append('L ' + str(seed) + ' ' + ' '.join(ops))
        lines.append('X ' + str(seed) + ' ' + ' '.join(ops))
    mix_values = [0, 1, -1, 2147483647, -2147483648, 9223372036854775807, -9223372036854775808]
    mix_values += [rng.randrange(-(1 << 63), 1 << 63) for _ in range(20000)]
    for v in mix_values:
        lines.append('mix ' + str(v))

    # Interleave all draw widths and cached/uncached Gaussians, then reseed
    # with a populated cache. Fg exercises fresh fork cache and parent cache.
    floating = ['f', 'd', 'g', 's', 'i', 'f', 'g', 's', 'g', 'Fg', 'g', 'g',
                'T:0:4607182418800017408', 't:0:1065353216',
                'g', 'S:42', 'g', 'g', 'S:-1', 'f', 'd', 'g', 's']
    for seed in seeds + [rng.randrange(-(1 << 63), 1 << 63) for _ in range(4000)]:
        for kind in ('L', 'X'):
            lines.append(f'{kind} {seed} ' + ' '.join(floating))
    # Long runs force rejection as well as both cache phases. Every Gaussian
    # is followed by state words, so accidentally advancing on cache hits fails.
    for seed in seeds[:15]:
        for kind in ('L', 'X'):
            lines.append(f'{kind} {seed} ' + ' '.join(['g', 's'] * 200))
    # Triangles preserve operation width, evaluation order and IEEE specials.
    doubles = [0, -(1 << 63), 0x3ff0000000000000, 0x7fefffffffffffff,
               0x7ff0000000000000, 0x7ff8000000000000, 1]
    singles = [0, 0x80000000, 0x3f800000, 0x7f7fffff, 0x7f800000, 0x7fc00000, 1]
    for _ in range(4000):
        doubles.append(rng.randrange(-(1 << 63), 1 << 63))
        singles.append(rng.getrandbits(32))
    for i, mean in enumerate(doubles):
        deviation = doubles[(i * 17 + 3) % len(doubles)]
        fm, fd = singles[i], singles[(i * 11 + 1) % len(singles)]
        seed = rng.randrange(-(1 << 63), 1 << 63)
        ops = f'T:{mean}:{deviation} t:{fm}:{fd} d f g g s'
        for kind in ('L', 'X'):
            lines.append(f'{kind} {seed} {ops}')

    data = ('\n'.join(lines) + '\n').encode()
    (out / 'random_fixtures.txt').write_bytes(data)
    expected = run([out / 'random_oracle'], input=data)
    (out / 'random_expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'random_{mode}', root / 'goose/random_probe.goose'])
        actual = run([out / f'random_{mode}'], input=data)
        (out / f'random_{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            mismatch = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            raise SystemExit(f'Random {mode} divergence at output line {mismatch + 1}; inspect transcripts in {out}')
    report = {'module': 'core/random', 'revision': lock['revision'], 'source_sha256': digest,
              'fixture_lines': len(lines), 'output_lines_compared_per_build': len(expected.splitlines()),
              'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'], 'original_unit_tests': original_tests,
              'oracle_exclusions': [text for text, _ in EXCLUSIONS],
              'pending': ['positional.rs, worldgen.rs, hash.rs submodules'],
              'jmath_evidence': 'jmath-report.json',
              'jmath_source_sha256': jmath_digest,
              'float_comparison': 'raw IEEE-754 bits, including signed zero and NaN',
              'rustc_version': run([args.rustc, '--version']).decode().strip()}
    (out / 'random-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(lines)} random fixture lines, '
          f'{len(expected.splitlines())} output lines; Rust/Goose match in debug/release (integer and floating-point streams).')
    return report
