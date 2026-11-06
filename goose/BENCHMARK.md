# Section palette construction benchmark

Measured on 2026-11-05, AMD Ryzen 7 5825U with Radeon Graphics, x86_64,
Linux. Rust: `rustc 1.98.1 (48a229cea 2026-09-01)`; Goose: `goose runtime 4d3da949e563`;
C compiler: `cc (Gentoo 16.2.1_p20260926 p1) 16.2.1 20260926`. Reference revision is recorded with every sample
in [the full report](benchmarks/palette-gentoo-x86_64.json).

This times `Paletted::from_values` and a complete output checksum for one
4,096-entry section, using the original Rust implementation and the translated
Goose implementation. Each process performs four warmups before timing. Seven
samples alternate language execution order; the table reports medians per call.
The ordinary dense cases use 16 global bits; the deliberately sparse fallback
uses 32. Rust uses `rustc -O`; Goose uses its native release build.

| Input case | Global bits | Rust | Goose | Rust time / Goose time |
| --- | ---: | ---: | ---: | ---: |
| dense-list | 16 | 8.54 µs | 6.14 µs | 1.39× |
| dense-global | 16 | 18.53 µs | 10.43 µs | 1.78× |
| sparse-fallback | 32 | 446.14 µs | 61.02 µs | 7.31× |

`dense-list` cycles through 20 state IDs. `dense-global` has 4,096 distinct dense
IDs. `sparse-fallback` has distinct IDs >= 1,000,000, outside normal vanilla 26.3
registry IDs. The latter exercises the dictionary replacing Rust's quadratic
linear palette search. All cases modify the first input entry between calls.
Each pair must produce the same checksum before its timing can be accepted;
the separate parity suite checks exact palette and wire output.

These are CPU kernel measurements on a shared host, with load and all raw
samples recorded. They do not establish game FPS, a whole-client speedup, memory
improvement, equivalence to vanilla or suitability for low-end PCs. The sparse
case is not representative of ordinary vanilla block IDs.

Reproduce from the repository root:

```sh
python3 goose/tools/bench_net.py --goose "$HOME/.local/bin/goose"
```

The tool rejects a reference revision/hash mismatch or output checksum mismatch.
Generated executables, source snapshots and the new full report go to
`build-goose/port/benchmark/`. It requires no network or Cargo dependencies.
