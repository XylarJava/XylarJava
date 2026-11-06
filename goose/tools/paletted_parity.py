"""Fixture streams for the unchanged reference palette module and its Goose port."""
import hashlib
import json
import random
from pathlib import Path


def signed(word):
    word &= (1 << 64) - 1
    return word if word < (1 << 63) else word - (1 << 64)


def packed_words(bits, values, padding=False):
    if bits == 0:
        return []
    per = 64 // bits
    result = []
    for start in range(0, len(values), per):
        group = values[start:start + per]
        word = sum(v << (i * bits) for i, v in enumerate(group))
        if padding and len(group) * bits < 64:
            word |= ((1 << 64) - 1) ^ ((1 << (len(group) * bits)) - 1)
        result.append(signed(word))
    return result


def fixtures():
    rng = random.Random(0x50414C45545445)
    lines = []

    def wire(kind, glob, bits, palette, words):
        fields = ['W', kind, glob, bits, len(palette), len(words), *palette, *words]
        lines.append(' '.join(map(str, fields)))

    def saved(kind, glob, palette, present, words):
        fields = ['P', kind, glob, len(palette), int(present), len(words), *palette, *words]
        lines.append(' '.join(map(str, fields)))

    # Resize at each linear/hash/global boundary, including removal of stale ids.
    for kind, glob in [(0, 16), (1, 7)]:
        count = 4096 if kind == 0 else 64
        maximum = 301 if kind == 0 else 11
        lines += [f'N {kind} {glob} 0', 'D']
        for n in range(1, maximum):
            lines.append(f'S {n} {n}')
            if n in [1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 300]:
                lines += ['D', f'H {n} {n + 1}', 'J 3 0', 'I 0']
        for i in range(count):
            lines.append(f'S {i} 0')
        lines += ['I 0', 'D', 'H 1 1', 'R', 'D', f'E {1 - kind} {glob} 0']

    # Observable old values under exactly the mixed-update workloads used by
    # the reference unit test, with intermediate wire/save snapshots as well.
    for kind, glob, distinct in [(0, 16, 700), (0, 16, 12), (1, 7, 9), (1, 7, 3)]:
        count = 4096 if kind == 0 else 64
        lines.append(f'N {kind} {glob} 5')
        for edit in range(20000):
            lines.append(f'S {rng.randrange(count)} {rng.randrange(distinct) * 7}')
            if edit % 2000 == 0:
                lines += ['D', 'H 0 7', 'J 5 2']
        lines += ['D', 'R', 'D']

    # Direct creation: first occurrence order, repeated runs, dense and sparse
    # lookups. Sparse ids stay list-local unless the global width can hold them.
    for kind, glob in [(0, 16), (1, 7), (0, 32)]:
        count = 4096 if kind == 0 else 64
        for distinct in [1, 2, 5, 8, 9, 16, 17, 32, 129, 256, 257, 600]:
            actual = min(distinct, count)
            values = [rng.randrange(actual) for _ in range(count)]
            if glob == 32:
                values = [70000 + v * 100003 for v in values]
            lines += [f'V {kind} {glob} ' + ' '.join(map(str, values)), 'D', 'R', 'D', 'J 7 3']
        sparse = [70000 + i % 40 for i in range(count)]
        lines += [f'V {kind} {glob} ' + ' '.join(map(str, sparse)), 'D']

    # Every leading bit byte configuration and noncanonical/padded raw words.
    for kind, glob in [(0, 16), (1, 7)]:
        count = 4096 if kind == 0 else 64
        limit = 8 if kind == 0 else 3
        for declared in [*range(33), 255]:
            bits = 0 if declared == 0 else (max(4, declared) if kind == 0 and declared <= 8
                                           else declared if kind == 1 and declared <= 3 else glob)
            palette = [17] if declared == 0 else [17, 18, 19]
            # A two-entry biome palette when the declared width is one.
            if bits == 1:
                palette = palette[:2]
            ids = [i % len(palette) for i in range(count)]
            words = packed_words(bits, ids, padding=True)
            wire(kind, glob, declared, palette, words)
            lines += ['D', 'H 17 18', 'I 17', 'R', 'D']
            if bits:
                wire(kind, glob, declared, [], words[:-1])
                wire(kind, glob, declared, [], words)
                if declared <= limit:
                    # Invalid entries in several words; the earliest error wins.
                    bad = ids[:]
                    bad[0] = (1 << bits) - 1
                    bad[-1] = (1 << bits) - 1
                    if bad[0] >= len(palette):
                        wire(kind, glob, declared, palette, packed_words(bits, bad, padding=True))
        wire(kind, glob, 0, [1, 2], [])
        wire(kind, glob, 0, [], [0])
        lines.append('D')

    # Saved formats: single-value ignores data, absent data differs from an
    # empty provided array, global saves remap local ids into registry ids.
    for kind, glob in [(0, 16), (1, 7)]:
        count = 4096 if kind == 0 else 64
        local_limit = 256 if kind == 0 else 8
        for length in [0, 1, 2, 3, 4, 8, 9, 16, 17, 128, 256, 257, 600]:
            palette = [((i * 37) % (60000 if kind == 0 else 100)) for i in range(length)]
            bits = 0 if length <= 1 else ((max(4, (length - 1).bit_length()) if kind == 0
                                         else (length - 1).bit_length()))
            ids = [i % length for i in range(count)] if length else []
            words = packed_words(bits, ids, padding=True)
            saved(kind, glob, palette, True, words)
            lines += ['D', 'J 3 1', 'R', 'D']
            saved(kind, glob, palette, False, [])
            saved(kind, glob, palette, True, [])
            if length > 1:
                saved(kind, glob, palette, True, words + [0])
                if length < 1 << bits:
                    for bad_index in [0, 64 // bits, count - 1]:
                        bad = ids[:]
                        bad[bad_index] = length
                        bad[-1] = (1 << bits) - 1
                        saved(kind, glob, palette, True, packed_words(bits, bad, padding=True))
        saved(kind, glob, [9], False, [123, -1])
        lines += ['D', f'E {kind} {glob} 9', f'E {1 - kind} {glob} 9']

    # Global integer widths, including unsigned state ids and nonzero padding.
    for kind in [0, 1]:
        count = 4096 if kind == 0 else 64
        for bits in [1, 3, 4, 5, 7, 9, 15, 16, 17, 31, 32]:
            maximum = (1 << bits) - 1
            values = [rng.randrange(min(maximum, 500) + 1) for _ in range(count)]
            if bits == 32:
                values[0] = (1 << 32) - 1
            wire(kind, bits, 255, [123, 456], packed_words(bits, values, padding=True))
            lines += ['D', 'H 0 7', 'J 5 2', 'R', 'D']

    # Duplicate palette entries are accepted by the source, not normalized.
    wire(0, 16, 4, [7, 7], packed_words(4, [i % 2 for i in range(4096)], padding=True))
    lines += ['D', 'I 7', 'H 7 7', 'R', 'D']
    return ('\n'.join(lines) + '\n').encode(), len(lines)


def check_paletted(args, reference, lock, out, run):
    sources = {'@STORAGE_SOURCE@': 'crates/horizon-core/src/storage.rs',
               '@PALETTED_SOURCE@': 'crates/horizon-core/src/paletted.rs'}
    root = Path(__file__).resolve().parents[2]
    template = (root / 'goose/tools/paletted_oracle.rs.in').read_text()
    for placeholder, name in sources.items():
        path = reference / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != lock['sources'][name]:
            raise SystemExit(f'Palette reference hash mismatch: {name}')
        template = template.replace('"' + placeholder + '"', json.dumps(str(path)))
    (out / 'paletted_oracle.rs').write_text(template)
    print('Compiling original palette tests and translated container...', flush=True)
    run([args.rustc, '--edition=2024', '--test', '-O', out / 'paletted_oracle.rs', '-o', out / 'paletted_original_tests'])
    original_tests = run([out / 'paletted_original_tests']).decode()
    print(original_tests, end='')
    run([args.rustc, '--edition=2024', '-O', out / 'paletted_oracle.rs', '-o', out / 'paletted_oracle'])
    run([args.goose, 'build', '--debug', '-o', out / 'paletted_translated_tests', root / 'goose/paletted_test.goose'])
    print(run([out / 'paletted_translated_tests']).decode(), end='')
    data, operations = fixtures()
    (out / 'paletted_fixtures.txt').write_bytes(data)
    expected = run([out / 'paletted_oracle'], input=data)
    (out / 'paletted_expected.txt').write_bytes(expected)
    for mode, flags in [('debug', ['--debug']), ('release', [])]:
        run([args.goose, 'build', *flags, '-o', out / f'paletted_{mode}', root / 'goose/paletted_probe.goose'])
        actual = run([out / f'paletted_{mode}'], input=data)
        (out / f'paletted_{mode}.txt').write_bytes(actual)
        if actual != expected:
            a, b = actual.splitlines(), expected.splitlines()
            mismatch = next((i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
            raise SystemExit(f'Palette {mode} divergence at output line {mismatch + 1}; inspect transcripts in {out}')
    report = {'module': 'core/paletted', 'revision': lock['revision'],
              'source_sha256': lock['sources'][sources['@PALETTED_SOURCE@']],
              'fixture_operations': operations, 'output_lines_compared_per_build': len(expected.splitlines()),
              'output_sha256': hashlib.sha256(expected).hexdigest(),
              'matched_builds': ['goose-debug', 'goose-release'], 'original_unit_tests': original_tests}
    (out / 'paletted-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {operations} palette fixture operations; exact Rust/Goose state, wire and saved data match in debug/release.')
    return report
