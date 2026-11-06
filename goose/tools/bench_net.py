#!/usr/bin/env python3
"""Compare the actual section palette kernel; reports no client/FPS speedup."""
import argparse
import json
import os
import platform
import shutil
import statistics
from datetime import datetime, timezone
from pathlib import Path
from check_port import run, ROOT
from net_parity import reference_snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--goose', default=shutil.which('goose'))
    parser.add_argument('--rustc', default=shutil.which('rustc'))
    parser.add_argument('--repeats', type=int, default=7)
    args = parser.parse_args()
    if not args.goose or not args.rustc or args.repeats < 3:
        parser.error('requires aardappel Goose, rustc, and at least 3 repeats')
    lock = json.loads((ROOT / 'goose/upstream.lock.json').read_text())
    reference = ROOT / '.cache/horizon-upstream'
    revision = run(['git', '-C', reference, 'rev-parse', 'HEAD']).decode().strip()
    if revision != lock['revision']:
        raise SystemExit('Reference revision does not match the lock')
    out = ROOT / 'build-goose/port/benchmark'
    out.mkdir(parents=True, exist_ok=True)
    reference_snapshot(ROOT, reference, lock, out)
    original = (reference / 'crates/horizon-protocol/src/lib.rs').read_text()
    template = (ROOT / 'goose/tools/net_bench.rs.in').read_text().replace('@LIB_TYPES@', original[original.index('/// Connection protocol states'):])
    for name, path in {'@BUF_SOURCE@': out / 'protocol_reference/buf.rs', '@CHUNK_SOURCE@': out / 'protocol_reference/chunk.rs'}.items():
        template = template.replace('"' + name + '"', json.dumps(str(path)))
    (out / 'net_bench.rs').write_text(template)
    run([args.rustc, '--edition=2024', '-O', out / 'net_bench.rs', '-o', out / 'rust-bench'])
    run([args.goose, 'build', '-o', out / 'goose-bench', ROOT / 'goose/net_bench.goose'])
    results = []
    for kind, (label, rounds) in enumerate([('dense-list', 10000), ('dense-global', 10000), ('sparse-fallback', 200)]):
        samples = {'rust': [], 'goose': []}
        for repeat in range(args.repeats):
            checksums = {}
            for language in (['rust', 'goose'] if repeat % 2 == 0 else ['goose', 'rust']):
                checksum, ns = map(int, run([out / f'{language}-bench', kind, rounds]).split())
                checksums[language] = checksum
                samples[language].append(ns / rounds)
            if checksums['rust'] != checksums['goose']:
                raise SystemExit('Benchmark checksum mismatch; rejecting timings')
        rust = statistics.median(samples['rust'])
        goose = statistics.median(samples['goose'])
        item = {'case': label, 'global_bits': 32 if kind == 2 else 16, 'calls_per_sample': rounds, 'rust_ns_per_call': samples['rust'],
                'goose_ns_per_call': samples['goose'], 'ratio_rust_over_goose': rust / goose}
        results.append(item)
        print(f'{label}: Rust {rust/1000:.2f} us, Goose {goose/1000:.2f} us, ratio {rust/goose:.3f}x', flush=True)
    cpu_model = platform.processor()
    if Path('/proc/cpuinfo').exists():
        cpu_model = next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                          if line.startswith('model name')), cpu_model)
    report = {'minecraft': '26.3', 'reference_revision': revision, 'repeats': args.repeats,
              'measured_at_utc': datetime.now(timezone.utc).isoformat(), 'cpu_model': cpu_model,
              'platform': platform.system(), 'architecture': platform.machine(), 'load_average': os.getloadavg(),
              'rustc': run([args.rustc, '--version']).decode().strip(),
              'goose': run([args.goose, 'version']).decode().strip(), 'results': results,
              'c_compiler': run(['cc', '--version']).decode().splitlines()[0],
              'scope': 'Single 4096-entry section palette construction kernel; not client FPS or end-to-end performance.',
              'method': 'Same inputs, four warmups, full output checksum, monotonic elapsed time, alternating execution order; median of repeated native release runs.',
              'limits': ['Shared host is not an isolated low-end benchmark machine.',
                         'Sparse fallback contains registry ids >= 1000000, outside normal vanilla 26.3 registry ids.',
                         'Rust -O and Goose default native release compiler settings are used; no game renderer/world simulation is measured.']}
    (out / 'net-benchmark.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Report:', out / 'net-benchmark.json')


if __name__ == '__main__':
    main()
