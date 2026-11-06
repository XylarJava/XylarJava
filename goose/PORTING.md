# Xylar Goose — Minecraft Java 26.3 port

Port the reference Rust implementation to Goose, retaining its behavior and
validating identical fixtures on both implementations. The separate finite
voxel sandbox is not part of this translation and does not count as port
coverage. Source provenance and required license notices are recorded in
[UPSTREAM.md](UPSTREAM.md).

## Current status

The reference is pinned in `upstream.lock.json`. The following units have
been translated into Goose and compared with the pinned Rust definitions:

| Reference unit | Goose implementation | Evidence |
| --- | --- | --- |
| Packed block/palette storage | `core/storage.goose` | 10,582 fixture operations; original and translated unit tests |
| Java decimal integer conversion | `core/text.goose` | 130,132 fixtures, including every BMP scalar |
| Biome world-seed hash | `core/seed_hash.goose` | 20,258 seeds, also checked against Python hashlib |
| Resident block/biome palettes | `core/paletted.goose` | 85,837 update/load/save operations, exact resident/wire/saved comparison |
| Scalar block/section/chunk positions | `core/position.goose` | 37,576 fixtures × 36 integer fields; box iterators still pending |
| Protocol integer/string/array primitives | `net/buf.goose` | Part of 75,229 protocol fixtures, including all 65,536 two-byte UTF-8 sequences |
| Handshake and status packets, protocol 777 | `net/packets.goose` | Same protocol fixtures; exact packet bodies, unknown payloads and errors |
| Network block/biome palettes and section counters | `net/chunk.goose` | Same protocol fixtures; exact wire bytes, truncated-input errors and reader positions |
| VarInt21 framing and compression policy | `net/frame.goose` | 74,742 fixtures against original Rust policy with an adapted zlib backend, plus a Python oracle |
| All login packet codecs and profiles | `net/login.goose` | 7,171 exact field/encoding/error comparisons; six clientbound and five serverbound variants |
| Legacy/Xoroshiro seeded random streams | `core/random.goose` | 530,288 output lines over 60,615 fixtures, including raw float/double/Gaussian/triangle bits, state, forks and reseeding; original Rust unit tests and a golden JDK integer sequence |
| CFB8 stream state, MD5 and offline UUIDs | `net/crypto.goose` | 8,463 exact Rust/Goose fixtures with an adapted AES backend; independent hashlib/UUID checks |
| Binary named/network NBT | `data/nbt.goose` | 100,626 raw-byte fixtures, original binary tests and translated writer regressions |
| All configuration packet codecs | `net/configuration.goose` | 10,100 canonical encoding/error fixtures and three original codec tests |
| Rounding, FdLibm kernels and x86-64 Linux math entry points | `core/jmath.goose` | 521,782 raw-bit fixtures, translated upstream golden vectors and signed-zero/scaling regressions |

Both debug and optimized Goose builds match the reference on these fixtures.
See [the symbol ledger](port-ledger.json) for mappings, adaptations and pending
subsystems. These units are **not yet connected to a ported game client**.

The random port covers seeded integer, float and double draws, cached
MarsagliaPolar Gaussians, both triangle widths, reseeding and forks. The oracle
keeps all these definitions verbatim, excluding only the positional/worldgen/hash
submodule declarations, re-exports and associated positional type/fork methods.
Mixed fixtures check that cache hits consume no draws, reseeding clears the
Gaussian cache, and forks start uncached without discarding the parent's cache.
Positional and worldgen sources and string/position seed hashing remain pending.

The `jmath` comparison currently supports **x86-64 Linux**. Rounding saturates
like Rust casts, including NaN-to-zero. FdLibm log/log10/cbrt/hypot are translated
kernels; most platform entry points call system libm. In this environment,
**rustc 1.98.1** resolves `f64::cbrt` to compiler-builtins' CORE-MATH routine
instead of glibc; that nearest-rounding routine is also translated into Goose.
Only bit reinterpretation (`memcpy`) and explicit fused multiply-add use native
helpers/libm. Its additional notice is in `licenses/core-math-MIT.txt`.

The Rust math snapshot substitutes unavailable software-libm and geometry calls
only in unexercised paths, listed in `jmath-report.json`. AArch64 sin/cos stubs,
Windows exact sin/cos and atan/asin/acos/atan2 remain pending. The harness does
not run those original math tests or certify other architectures. Embedded
upstream HotSpot-on-M4 vectors check the FdLibm kernels separately; the broader
comparison establishes pinned Rust equivalence, **not** independent vanilla or
HotSpot-intrinsic equivalence. Toolchain-dependent math behavior is recorded in
the report and a different toolchain may require another adaptation.

The storage port retains an observed reference defect: on x86, a 31-bit storage
of length 16,384 aliases indices 0 and 8,192 through a wrapping multiply. This
is explicitly reproduced by the comparison harness, not certified as correct
Minecraft behavior. Normal 4,096-entry sections and the division fallback above
16,384 entries avoid that domain. Do not silently change the reference behavior
inside a claimed exact port; any corrected indexing policy needs a separate
change and comparison with vanilla.

`client.goose`, `world.goose`, `player.goose` and `native/` remain the independent
sandbox. The existing prototype and primitive tests do not count toward Rust port coverage.
`protocol.goose` remains an independent primitive, not a ported protocol module.

## Reproduce port verification

To manually exercise the translated modules after building `goose_port_demo`:

```sh
./build-goose/xylar-goose-port integer '١٢٣'
./build-goose/xylar-goose-port seed 42
./build-goose/xylar-goose-port storage 5 1 31 7
```

The first command prints `123`; the storage command packs then reads back
`1 31 7`. `--help` lists the commands. Invalid input exits with status 2.
This terminal tool directly calls the translated code; it does not launch the
separate voxel sandbox or connect to a Minecraft server.

Requires the pinned local checkout, aardappel Goose, rustc, Python 3 and a C
compiler and system zlib development files. No Cargo downloads, game jars,
prepared resources or accounts are used.

```sh
python3 goose/tools/check_port.py --goose "$HOME/.local/bin/goose"
```

The tool checks the source revision and SHA-256 hashes before compiling the
original function bodies through small Rust wrappers. The scalar position
oracle explicitly removes only the two imports of its unported iteration
submodule; scalar definitions and their three original tests are unchanged. It runs the original
storage tests, translated golden tests, identical fixture streams on both
languages, and debug/optimized Goose comparisons. Results, transcripts and
machine-readable reports go to `build-goose/port/`.

The network oracle retains the original handshake/status definitions, packet
macro behavior and the `Paletted`/`Section` implementation bodies. Its explicit
exclusions are NBT-dependent buffer methods, other packet states and chunk data
after `Section`. The report lists these exclusions. The 12 original protocol
tests also exercise some Rust floating-point helpers that are **not yet ported**;
only the compared primitive, packet and section fixtures establish Goose coverage.
Strings count UTF-16 units, preserve strict UTF-8 errors and borrow their input.
Chunk sections preserve both the non-empty-block and fluid counters. Network
palette validation intentionally follows the wire source, independently from
the stricter resident palette container.

The framing port preserves the 21-bit frame quota, the separate 8 MiB decoded
quota, threshold checks, noncanonical VarInts and inflater recovery after bad
input. Only the compression-library interface is C (`native/compression.c`);
packet framing and validation are Goose. The Rust oracle keeps `frame.rs`
verbatim and substitutes its unavailable Cargo `flate2/zlib-rs` dependency with
an explicit system-zlib adapter. Five original frame tests pass under this
adapter, and Python independently checks framing results using Python zlib.
This is **not** a comparison against upstream zlib-rs's compressed byte choices.
Backend-specific error diagnostics use numeric zlib status. An inflater has
exclusive ownership and must be closed explicitly; C retains no borrowed Goose
buffer pointers between calls.

Login comparisons cover profile properties/signatures, UTF-16 string limits,
both UUIDs in `LoginFinished`, optional empty payloads, unknown packets and
trailing-byte rejection. Two original login tests are run with one documented
fixture substitution: the unported offline-UUID helper call becomes a fixed
UUID; no crypto behavior is certified by that test. A Goose integration test
negotiates compression in the previous framing mode, then reads both compressed
and below-threshold login packets. These are codecs, not an authenticated
connection by themselves: the separately tracked Linux diagnostic now integrates
transport and adapted RSA/AES. Microsoft session joining remains pending.

For the port-only suite (**22/22 passed**, including the native compression/crypto
bindings, stream integration and comparison harness):

```sh
cmake -S goose -B build-goose -DGOOSE_TEST_PORT_PARITY=ON \
  -DGOOSE_BUILD_SANDBOX=OFF -DGOOSE_EXECUTABLE="$HOME/.local/bin/goose"
cmake --build build-goose -j 1
ctest --test-dir build-goose --output-on-failure
```

The old voxel sandbox is now excluded from the normal build. Its separate
SDL/OpenGL backend and graphics tests require explicit `GOOSE_BUILD_SANDBOX=ON`
and `GOOSE_TEST_GRAPHICS=ON`; they are not port coverage.

The compression C binding also passed AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cc -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer goose/native/compression_test.c \
  goose/native/compression.c -lz -o build-goose/compression-sanitize
ASAN_OPTIONS=detect_leaks=0 ./build-goose/compression-sanitize
```

LeakSanitizer cannot run under this environment's ptrace supervisor; leak
detection is disabled in that command. It is not a leak audit.

The resident palette adaptation uses one growable owner: raw words followed by
list ids. This fits Goose's one-resizable-final-field rule without reserving a
256-entry list in every single-value section. List ids occupy u64 backing slots;
wire/saved forms remain byte-for-byte equivalent. `to_wire` returns an owned
snapshot rather than borrowed words. The network palette builder uses a sparse
dictionary in place of the reference's linear fallback while preserving palette
order. Its native CPU benchmark, including all sampled cases, is documented in
[BENCHMARK.md](BENCHMARK.md). Whole-client memory, FPS and low-end comparisons
remain pending.

## Native connection milestone

The normal Linux build now produces `build-goose/xylar-goose-network`:

```sh
./build-goose/xylar-goose-network status localhost 25565
./build-goose/xylar-goose-network login-offline localhost 25565 Xylar
```

The second command reaches **only the configuration boundary**, immediately
after `LoginAcknowledged`, then closes the connection. It does not load a world.
It supports compression, login queries, empty-cookie replies, and optional
RSA/CFB8 encryption when `should_authenticate` is false. Requests for Microsoft
session authentication fail explicitly; no online-mode capability is claimed.

AES forward blocks, entropy and RSA public-key operations use OpenSSL. CFB8
register state, MD5, UUID generation, packet ordering, compression negotiation
and buffered-read handling remain Goose. The Rust crypto subset is compared
on 8,463 fixtures, including raw stream bytes and final cipher registers.
The oracle explicitly adapts its unavailable AES dependency; RSA arithmetic,
server hashes and secure-chat signatures are not covered by that comparison.

Debug and release integration suites each perform 25 stream scenarios, including
RSA 1024/2048 exchange, coalesced compression transitions, empty frames, malformed
input, disconnects, EOF and timeouts. Each also exchanges 90 encrypted/compressed
packets up to 200 KB under two read limits. The independent peer uses Python
framing/zlib/hashlib and OpenSSL CLI, not a Minecraft server or the Rust client.

The test process communicates through OS pipes because this environment denies
socket creation. This substitutes **only the byte-stream backend**. The Linux
TCP backend is compiled separately; the harness attempts a loopback status check
where sockets are permitted and records `unavailable` with the OS reason here.
Consequently, passing CTest here does not certify live TCP connectivity, a real
26.3 server join, or the full original Rust connection implementation.
Reports are `build-goose/session-{debug,release}-report.json`.

The native crypto adapter also passes AddressSanitizer/UndefinedBehaviorSanitizer
with leak detection disabled for this environment. Native tests check FIPS-197,
RSA private-operation interoperability, key types, invalid input and output
buffer bounds. It is not a leak audit.

Build requirements now include OpenSSL development headers/library and the
`openssl` CLI, plus Python 3 for stream integration even without Rust parity.
The next gameplay milestones are in [PLAYABLE.md](PLAYABLE.md).

## Binary NBT and configuration milestone

`data/nbt.goose` now reads/writes binary named and network NBT. Its complete Rust
source is compiled unchanged as the oracle. The 100,626 compared fixtures cover
all tag types, quotas, truncations, all 65,536 two-byte modified-UTF inputs,
unpaired surrogates, overlong strings, NaN policies, duplicate keys and mixed
list wrappers. Original binary tests run separately, excluding SNBT, streaming
visitors and the large-template preset test. A translated writer regression also
constructs mixed trees and overlong strings directly.

An owning u64 arena replaces recursive enum/BTreeMap allocation. Node links and
data spans are offsets, and compound entries are kept in UTF-16 key order.
Byte/UTF-16 arrays therefore take more memory than Rust; this is an explicit
adaptation, not a memory-performance claim. SNBT, streaming visitors and trusted
file-limit presets remain pending.

All 19 clientbound and 10 serverbound configuration variants are translated.
Their NBT fields are canonical validated network bytes, convertible to an arena
on demand. Optional flags, nested lists, ClickPayload's bounded sub-reader,
unknown packets and trailing-byte rejection match the original codecs over
10,100 fixtures. Three original configuration tests pass; the test-only
`text::literal` fixture helper makes an NBT String rather than certifying text
component construction.

```sh
./build-goose/xylar-goose-network configure-offline localhost 25565 Xylar
```

This mode continues after login: sends brand/client information, replies to
keep-alives and pings, offers no prepared known packs, retains registry/tag/NBT
batches and cookies, and acknowledges `FinishConfiguration`. The diagnostic
then closes at the play boundary. It derives dimension layouts and biome palette width but does not load or render
a running world. Resource packs are declined; code-of-conduct
acceptance, interactive dialogs and transfer return explicit unsupported errors.

The debug/release stream suites now have 40 scenarios each. Configuration is
also tested after encrypted/compressed login, and a coalesced first play packet
remains available after the configuration acknowledgement. Live TCP/server
verification is still unavailable in this socket-restricted sandbox.


## Chunk data and initial Play transport

The entire wire `chunk.rs` is translated: heightmaps, column buffers, block
entity NBT, section extraction and sky/block lighting. The oracle retains the
complete original chunk, buffer and NBT implementations. It also extracts fifteen
unchanged Play packet declarations/enum rows for the terrain/control subset.
37,236 fixtures compare debug/release canonical bytes, cursor/error behavior,
section reconstruction and every expanded light byte. Seven original chunk tests
run unchanged. The independent test checks nibble ordering and mask precedence.

The Goose light expander walks layers once and keeps borrowed views in temporary
tables; each output layer is copied only into its final owner. This avoids the
intermediate full-layer copies of the initial translation. No FPS, memory or
speed advantage over Rust is claimed without a benchmark.

`terrain-offline HOST PORT NAME` negotiates login/configuration and attempts to
decode the first terrain packet, replying to keep-alives, pings and batch finish.
It has a packet bound and the existing read timeout. It is not a gameplay loop:
player login and teleport acknowledgements now run, while multi-column world storage,
loading completion and real-server verification remain pending. Respawn world
replacement, reconfiguration and disconnect stop explicitly. Tests include compression, byte-at-a-time transport, coalesced phase
transition, negative chunk coordinates and malformed/trailing chunk data.
Actual server TCP and graphical testing remain unavailable in this environment.


## Porting workflow

Read the reference instructions, retain LICENSE and applicable third-party
notices, and inventory source modules, dependency boundaries and tests from the
pinned checkout. Preserve that checkout for comparisons rather than rebranding
or editing the original source tree.

Create a module-by-module ledger containing Rust path/symbol, Goose path/symbol,
reference revision, status and parity-test evidence. Translate real source
behavior and run the same fixtures on both implementations. A unit that merely
compiles is not yet a verified port.

Begin with core types and protocol primitives, then data formats/world storage,
resource preparation, simulation and rendering. Graphics, concurrency, ownership
and ecosystem dependencies need explicit adaptation to Goose. Identify any
C/Rust bridges separately from translated Goose implementation.

Only after a version-specific port passes its comparison tests may the launcher
advertise Minecraft Java 26.3 as supported. The current project scope is only
26.3; earlier versions are outside this port. Account handling and performance
claims require their own end-to-end verification.

## Player synchronization

`net/player_sync.goose` translates PositionSender and teleport_velocity from the
complete pinned `player_sync.rs`, plus the pose/acknowledgement subset of
`game/remote.rs::remote_teleport`. It keeps f32 rotation arithmetic, relative
coordinates, pitch clamping and the original packet selection threshold and
20-update reminder. Teleport acknowledgements do not advance movement baselines.
The terrain diagnostic tracks these values but has no simulation/render tick.

The oracle compiles original player_sync.rs with original movement codec macros,
full original trig.rs, and extracted unchanged Vec3 constructor/rotation methods.
36,085 input lines and every sine-table entry match debug/release. Arithmetic
velocity NaNs are normalized in the comparison, not in production; finite values,
infinities and signed zero compare bitwise. The platform scope is x86-64 Linux.
Goose's table initializes once per worker (256 KiB each); this is not a measured
performance advantage over Rust's shared OnceLock.

40 independent stream scenarios cover all 512 teleport flags, compression,
fragmentation and malformed login/position packets. PlayerLoaded is deliberately
not sent: no resident world or rendered player section exists yet. Respawn codecs
are translated, while replacing a live world on respawn is still unsupported.

## Configured dimension and column binding

`net/registry.goose` translates DimensionType layout/defaults/shape validation
and the corresponding Dynamic.dimension fields. 6,474 fixtures compare the
unchanged pinned Dynamic/DimensionType implementation using original binary NBT.
Only layout fields are covered: default_clock, names, full Dynamic APIs, dialog
tags and built-in resource reports are not part of this port yet. Repeated
configuration registry batches use the last batch, including empty replacement.

The first-column diagnostic uses this layout after Play Login. `net/column.goose`
is a new ownership adapter over compared section/light codecs, not the Rust
renderer's World implementation. It owns compressed palettes, expanded light
layers (including the below/above layers) and block entities. Queries accept
local X/Z and absolute Y. Section traversal is currently linear; no performance
advantage is claimed. Diagnostic block palette width is pinned at 16; production
resource-derived block registry sizing remains pending. Biome width comes from
the latest server biome registry length.

The independent test covers negative minimum Y, section boundaries, block and
biome queries and truncated section rejection. Stream scenarios cover both the
24-section Overworld fallback and a custom two-section dimension whose earlier
registry batch is replaced. The diagnostic checks the column then closes; it
does not retain a running world, send PlayerLoaded or render terrain.

The terrain packet subset includes BlockUpdate, SectionBlocksUpdate and
ForgetLevelChunk. Packed positions retain every wire bit; section changes expose
absolute block coordinates and signed state IDs using the compared position
helpers. The codec comparison covers truncations, trailing bytes, signed states
and invalid collection/VarLong lengths. Applying these packets to a running
multi-column world is still pending.
