# Xylar Goose — Minecraft Java 26.3 port

Xylar Goose translates the pinned Rust client implementation into aardappel
Goose. The current target is **26.3 only**. The green Goose selector in the
launcher shows that target; installation remains unavailable while gameplay,
rendering and multiplayer sessions are being ported.

Translated and verified units:

- Packed storage, resident block/biome palettes and scalar coordinates.
- Java decimal integer parsing and biome seed hashing.
- Legacy (java.util.Random) and Xoroshiro128++ integer/float/double streams, cached Gaussians, triangles, reseeding and forks.
- Rounding, FdLibm kernels and x86-64 Linux math entry points, including a Goose CORE-MATH cube root matching rustc 1.98.1.
- Protocol 777 integer/string/array primitives and handshake/status/login packets.
- Network palette packing and section counters, including truncated-input errors.
- VarInt21 framing and compression policy, with a system-zlib C binding.
- CFB8/MD5/offline UUIDs, compared with Rust using an adapted OpenSSL AES backend.
- Binary NBT with lossless UTF-16, mixed lists, named/network output and quota checks.
- All configuration codecs, including registry/tag lists and NBT components.
- Linux status/login/configuration diagnostics, integration-tested with optional RSA/AES and compression.

The [symbol ledger](port-ledger.json) records the exact Rust-to-Goose mappings,
adaptations and remaining work. [PORTING.md](PORTING.md) describes coverage and
oracle exclusions; [UPSTREAM.md](UPSTREAM.md) preserves source and license notices.
These modules do not yet form a playable client.

## Build and verify on Gentoo

The installed aardappel Goose compiler is `~/.local/bin/goose` (v2026.3,
runtime `4d3da949e563`). Keep its extracted directory in `~/Downloads/goose/`
intact: it supplies the runtime and standard library. Native compilation needs
a C compiler, system zlib and OpenSSL headers/library, and the `openssl` CLI.
Stream integration needs Python 3. Comparison tests additionally need rustc and the pinned
reference checkout at `.cache/horizon-upstream`; no Cargo downloads are needed.

From the repository root:

```sh
cmake -S goose -B build-goose -DGOOSE_TEST_PORT_PARITY=ON \
  -DGOOSE_BUILD_SANDBOX=OFF -DGOOSE_EXECUTABLE="$HOME/.local/bin/goose"
cmake --build build-goose -j 1
ctest --test-dir build-goose --output-on-failure
```

The embedded TinyCC runner encounters a 32-bit libc linker script on this
Gentoo multilib installation. CMake therefore uses native `goose build`
executables. Debug and optimized translations are compared with the same Rust
fixtures: 75,229 primitive/section cases, 74,742 framing cases and 7,171 login
cases. Framing comparisons explicitly adapt the Rust compression dependency to
system zlib; see [the verification scope](PORTING.md). Reports stay in
`build-goose/port/`. The full CTest suite has 25 tests. Math adds 521,782 raw-bit fixtures; random
adds 60,615 mixed fixtures with 530,288 output lines per build. Math verification
is currently limited to x86-64 Linux and pinned Rust equivalence; other platform
paths and inverse trigonometric functions remain pending. Binary NBT adds
100,626 fixtures; configuration adds 10,100 fixtures and three upstream tests.
Full chunk/light data and fifteen play variants add 37,236 fixtures and seven upstream tests.

## Native network diagnostic

```sh
./build-goose/xylar-goose-network status localhost 25565
./build-goose/xylar-goose-network login-offline localhost 25565 Xylar
./build-goose/xylar-goose-network configure-offline localhost 25565 Xylar
./build-goose/xylar-goose-network terrain-offline localhost 25565 Xylar
```

The login diagnostic stops after acknowledgement; `configure-offline` also
collects registry/tag/NBT data and acknowledges configuration, then closes at the
play boundary. It does not render a game or interpret a world. Optional encryption for unauthenticated servers uses OpenSSL
RSA and Goose CFB8; Microsoft session joining is not implemented.

`terrain-offline` answers play keep-alives/pings and decodes the first complete
chunk with lighting. It tracks player login, applies teleport pose/velocity and acknowledges corrected
position/rotation, then stops at the first chunk. The first column binds to the server dimension layout; multi-column storage, loading completion,
respawn and rendering remain pending. This is a protocol diagnostic, not a playable
client. The stream suite covers 40 scenarios, including all 512 teleport flags.
Movement selection and velocity add 36,085 fixtures plus all 65,536 sine-table
entries compared to pinned Rust in debug/release on x86-64 Linux.

The debug/release stream tests run through subprocess pipes. Socket creation is
forbidden in this agent environment, so the separate live TCP check records
`unavailable`; it runs automatically where local sockets are allowed. Passing
these tests does not establish a real Minecraft-server join. See the precise
[verification scope](PORTING.md#native-connection-milestone) and
[the milestones toward gameplay](PLAYABLE.md).

Dimension layout adds 6,474 Rust comparisons, including invalid shape fallback,
NBT type handling and ambient-light bits. Configuration honors replacement of a
registry by its last batch. The column ownership adapter keeps section palettes
compressed and exposes block/biome queries at absolute Y. The diagnostic closes
after checking its first column; it is not a live world store or renderer.

## Performance

[The palette benchmark](BENCHMARK.md) measures the translated construction
kernel against the original Rust implementation with identical inputs and
checked output. It records all cases and samples. This does not measure client
FPS, loading time, memory consumption or low-end hardware suitability.

## Earlier prototype

The independent voxel sandbox is excluded from the normal build and from port
coverage. Its historical build instructions are retained in [SANDBOX.md](SANDBOX.md).
