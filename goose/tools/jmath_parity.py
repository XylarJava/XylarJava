"""Verify round_f32/round, the FdLibm kernels, and the libm entry points.

On x86-64 Linux the pinned Rust entry points use f64 methods. Most resolve
here to glibc; rustc 1.98.1's cbrt instead resolves to compiler-builtins'
CORE-MATH implementation, which is translated separately in Goose. FdLibm
kernels and rounding remain translations of the pinned jmath.rs definitions.
All floating results are compared by their raw bit patterns. This verifies
Rust equivalence, not an independent vanilla/HotSpot oracle.

Not exercised (tracked pending): the aarch64 and Windows `exact` sin/cos stubs,
atan2 (crate::geometry) and atan/asin/acos (the Rust software `libm` crate).
"""
import hashlib
import json
import math
import platform
import random
import struct
from pathlib import Path

# Standalone rustc has no `libm` crate and no crate::geometry. These calls live
# only in the aarch64/Windows and inverse-trig paths, which are not evaluated
# here; the x86-64 Linux else-branches are unchanged.
SUBSTITUTIONS = [
    ('        libm::tan(x)\n', '        x.tan()\n', 1),
    ('        libm::exp(x)\n', '        x.exp()\n', 1),
    ('        libm::pow(x, y)\n', '        x.powf(y)\n', 1),
    ('    libm::atan(x)\n', '    x.atan()\n', 1),
    ('    libm::asin(x)\n', '    x.asin()\n', 1),
    ('    libm::acos(x)\n', '    x.acos()\n', 1),
    ('        return if is_cos { libm::cos(x) } else { libm::sin(x) };\n',
     '        return if is_cos { x.cos() } else { x.sin() };\n', 1),
    ('    crate::geometry::trig::fdlibm_atan2(y, x)\n', '    y.atan2(x)\n', 1),
]


def db(v):
    """The signed i64 whose bits are the f64 v (how fixtures carry f64 bits)."""
    return struct.unpack('<q', struct.pack('<d', v))[0]


def fb(v):
    """The unsigned u32 whose bits are the f32 v."""
    return struct.unpack('<I', struct.pack('<f', v))[0]


def prepare_jmath(reference, lock, out):
    if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        raise SystemExit('jmath entry-point parity currently supports x86-64 Linux only')
    name = 'crates/horizon-core/src/jmath.rs'
    source = reference / name
    raw = source.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != lock['sources'][name]:
        raise SystemExit('jmath source hash differs from the pinned reference')
    snapshot = raw.decode()
    for old, new, expected in SUBSTITUTIONS:
        if snapshot.count(old) != expected:
            raise SystemExit('jmath external-call boundary changed; refusing to infer an oracle subset')
        snapshot = snapshot.replace(old, new)
    snapshot_path = out / 'jmath-subset.rs'
    snapshot_path.write_text(snapshot)
    return digest, snapshot_path


def check_jmath(args, reference, lock, out, run):
    root = Path(__file__).resolve().parents[2]
    digest, snapshot_path = prepare_jmath(reference, lock, out)
    template = (root / 'goose/tools/jmath_oracle.rs.in').read_text()
    (out / 'jmath_oracle.rs').write_text(template.replace('"@JMATH_SNAPSHOT@"', json.dumps(str(snapshot_path))))
    run([args.rustc, '--edition=2024', '-O', out / 'jmath_oracle.rs', '-o', out / 'jmath_oracle'])
    run([args.goose, 'build', '--debug', '-o', out / 'jmath_translated_tests', root / 'goose/jmath_test.goose'])
    print(run([out / 'jmath_translated_tests']).decode(), end='')

    rng = random.Random(0x4A4D415448)
    reals = [0.0, -0.0, 1.0, -1.0, 2.0, -2.0, 0.5, -0.5, 10.0, 100.0,
             math.pi, math.e, math.sqrt(2.0), 0.1, 0.25, 123456.789,
             1e-300, 1e300, 2.2250738585072014e-308, 5e-324,
             float('inf'), float('-inf'), float('nan'),
             1.0000000000000002, 0.9999999999999999, 3.141592653589793]
    # Random magnitudes across the exponent range, both signs.
    for _ in range(6000):
        m = rng.uniform(-1.0, 1.0)
        e = rng.randint(-320, 300)
        reals.append(math.ldexp(m, e))
    # Dense near the trig-reduction-sensitive region and small angles.
    for _ in range(3000):
        reals.append(rng.uniform(-50.0, 50.0))
    for _ in range(1500):
        reals.append(rng.uniform(-1e18, 1e18))

    # Raw patterns cover subnormals, full exponents, signed NaNs and payloads.
    reals += [struct.unpack('<d', struct.pack('<Q', rng.getrandbits(64)))[0]
              for _ in range(6000)]
    for exponent in range(1, 2047):
        reals.append(struct.unpack('<d', struct.pack('<Q', exponent << 52))[0])
    # CORE-MATH hard rounding cases, their neighbours and both signs.
    for pattern in (0x4009b78223aa307c, 0x401a202bfc89ddff, 0xf7f792b28f600000):
        for offset in (-1, 0, 1):
            v = struct.unpack('<d', struct.pack('<Q', pattern + offset))[0]
            reals.extend([v, -v])

    lines = []
    for v in reals:
        bits = db(v)
        for op in ('log', 'log10', 'exp', 'sin', 'cos', 'tan', 'sqrt', 'cbrt', 'flog', 'flog10', 'fcbrt'):
            lines.append(f'{op} {bits}')
    # pow and hypot over random pairs (and fixed interesting bases/exponents).
    for _ in range(6000):
        x = math.ldexp(rng.uniform(-1.0, 1.0), rng.randint(-40, 40))
        y = rng.uniform(-30.0, 30.0)
        lines.append(f'pow {db(x)} {db(y)}')
        lines.append(f'fhyp {db(x)} {db(y)}')
    # Comparable magnitudes force hypot scaling up/down (including the
    # subnormal branch), rather than taking its large-ratio early return.
    for exponent in (-1074, -1073, -1023, -1022, -1000, -600, -500, 500, 600, 1000, 1023):
        for scale in (1.0, 1.25, 1.75):
            x = math.ldexp(scale, exponent)
            y = math.ldexp(1.0, exponent)
            for sx in (x, -x):
                for sy in (y, -y):
                    lines.append(f'fhyp {db(sx)} {db(sy)}')
                    lines.append(f'fhyp {db(sy)} {db(sx)}')
    for v in reals:
        for w in (0.0, 1.0, -1.0, 2.0, 0.5, -0.5, 3.0, math.pi):
            lines.append(f'pow {db(v)} {db(w)}')
            lines.append(f'fhyp {db(v)} {db(w)}')
    # round(double): ties toward +inf, saturation and specials.
    round_cases = [-2.5, 2.5, -0.5, -1.5, 0.5, -2.6, 0.49999999999999994,
                   float('nan'), float('inf'), float('-inf'), 0.0, -0.0,
                   9.2e18, -9.2e18, 1e300, -1e300, 4503599627370496.5, 123.5, -123.5]
    for _ in range(4000):
        round_cases.append(math.ldexp(rng.uniform(-1.0, 1.0), rng.randint(-5, 65)))
    for v in round_cases:
        lines.append(f'rd {db(v)}')
    # round(float): exact, ties toward +inf, saturation, NaN.
    f32_cases = [0.49999997, 0.5, -0.5, 2.5, -2.5, 1e10, -1e10, float('nan'), 8388609.0,
                 -8388609.0, 0.0, -0.0, float('inf'), float('-inf'), 16777216.0, 0.4999999]
    for _ in range(4000):
        f32_cases.append(struct.unpack('<f', struct.pack('<I', rng.getrandbits(32)))[0])
    for v in f32_cases:
        if v != v or abs(v) == float('inf') or abs(v) < 1e30:
            lines.append(f'rf {fb(v)}')

    data = ('\n'.join(lines) + '\n').encode()
    (out / 'jmath_fixtures.txt').write_bytes(data)
    expected = run([out / 'jmath_oracle'], input=data)
    (out / 'jmath_expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'jmath_{mode}', root / 'goose/jmath_probe.goose'])
        actual = run([out / f'jmath_{mode}'], input=data)
        (out / f'jmath_{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            idx = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            raise SystemExit(f'jmath {mode} divergence at output line {idx + 1} (fixture: {lines[idx]}); '
                             f'inspect transcripts in {out}')
    report = {'module': 'core/jmath', 'revision': lock['revision'], 'source_sha256': digest,
              'fixture_lines': len(lines), 'output_lines_compared_per_build': len(expected.splitlines()),
              'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'],
              'platform': platform.platform(),
              'rustc_version': run([args.rustc, '--version']).decode().strip(),
              'cbrt_adaptation': 'Goose CORE-MATH nearest-rounding translation; rustc 1.98.1 compiler-builtins behavior',
              'comparison_limit': 'Pinned Rust equivalence, not independent vanilla/HotSpot certification',
              'oracle_substitutions': [old.strip() for old, _, _ in SUBSTITUTIONS],
              'verified': ['round_f32', 'round', 'fdlibm::{log,log10,cbrt,hypot}',
                           'Rust x86-64 Linux sin/cos/tan/exp/log/log10/pow/cbrt and sqrt entry points'],
              'pending': ['aarch64 sin/cos stub', 'Windows exact sin/cos', 'atan2 (crate::geometry)',
                          'atan/asin/acos (Rust software libm crate)']}
    (out / 'jmath-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(lines)} jmath fixture lines, {len(expected.splitlines())} output lines; '
          f'Rust/Goose match in debug/release.')
    return report
