# Source provenance and notices

This file records technical provenance and required acknowledgements; it is not
product branding. The product name is **Xylar Goose**.

- Source project: Horizon, `horizon-rust/horizon`.
- Repository: https://github.com/horizon-rust/horizon
- Reference revision: `f66e017f5ce2bfc3adc90f093c9250ec458c39ea`.
- Local reference checkout: `.cache/horizon-upstream` (ignored by Git).
- Source license: [MIT notice, preserved verbatim](licenses/upstream-MIT.txt).
- Copyright (c) 2026 Ludovic Marie and the Horizon contributors.

The MIT notice must accompany copies or substantial portions, including
translated source. Preserve applicable third-party notices when their code or
assets are incorporated. Keep the reference checkout intact for comparisons.
Neither renaming the product nor changing the implementation language removes
source attribution.

## Port status

The reference checkout is available locally. The following source units have
been translated, with copyright/license retained in each implementation:

- `crates/horizon-core/src/storage.rs` → `core/storage.goose` (functional API,
  with explicit memory-model and error-return adaptations).
- `crates/horizon-core/src/text.rs` → `core/text.goose`.
- `crates/horizon-core/src/biome/seed_hash.rs` → `core/seed_hash.goose`.
- `crates/horizon-core/src/paletted.rs` → `core/paletted.goose`.
- Scalar definitions of `crates/horizon-core/src/position.rs` → `core/position.goose`
  (the geometry-dependent iteration submodule is not yet ported).
- Integer/string/array primitives from `crates/horizon-protocol/src/buf.rs` →
  `net/buf.goose`; floating-point/velocity helpers and NBT remain pending.
- `Paletted` and `Section` from `crates/horizon-protocol/src/chunk.rs` →
  `net/chunk.goose`; outer chunk/light data and NBT remain pending.
- `crates/horizon-protocol/src/packets/handshake.rs` and `status.rs`, their
  packet-enum behavior and applicable IDs → `net/packets.goose` (protocol 777).
- `crates/horizon-protocol/src/frame.rs` → `net/frame.goose`. The external
  `flate2/zlib-rs` dependency is adapted to system zlib through
  `native/compression.c`; the verification oracle documents that substitution.
- `crates/horizon-protocol/src/packets/login.rs` and packet-enum behavior →
  `net/login.goose`, including `Property`, `GameProfile`, all six clientbound
  and five serverbound login codecs.

- `crates/horizon-core/src/random.rs` → `core/random.goose`, including seeded
  integer/float/double streams, MarsagliaPolar cache, triangles and reseeding;
  positional/worldgen/hash submodules remain pending.
- A subset of `crates/horizon-core/src/jmath.rs` → `core/jmath.goose`, covering
  rounding, FdLibm kernels and the verified x86-64 Linux entry points.

- `crates/horizon-protocol/src/crypto.rs` → CFB8/MD5/offline UUID subset in
  `net/crypto.goose`; OpenSSL supplies AES forward blocks and secure entropy.
- Buffering/framing policy from `crates/horizon-protocol/src/connection.rs` →
  `net/connection.goose`, with a native POSIX TCP/resolver adaptation. This is
  integration-tested, not a comparison against the whole Rust connection source.
- Status and offline login from `crates/horizon-client/src/net.rs` →
  `net/session.goose`. RSA public-key operations at the `rsa.rs` boundary are
  adapted to OpenSSL in `native/crypto.c`, not translated from its big integers.
  Login ends after acknowledgement; Microsoft session joining and the
  configuration diagnostics are tracked below; most play packets and gameplay remain unported.

- Binary reading/writing from `crates/horizon-world/src/nbt.rs` →
  `data/nbt.goose`, adapted to one owning arena. SNBT and streaming visitors are
  excluded from port coverage; the comparison oracle keeps the source unchanged.
- `crates/horizon-protocol/src/packets/configuration.rs` →
  `net/configuration.goose`, including all configuration variants and bounded
  click-action payloads. NBT fields retain validated canonical bytes.
- The configuration phase of `crates/horizon-client/src/net.rs` → diagnostic
  `session263::configure`, with explicit prepared-pack/UI/resource limitations.

The `cbrt` entry point additionally adapts the nearest-rounding CORE-MATH routine
from [rust-lang/compiler-builtins `libm/src/math/cbrt.rs`](https://github.com/rust-lang/compiler-builtins/blob/main/libm/src/math/cbrt.rs),
inspected on 2026-11-06, to match the installed rustc 1.98.1 implementation.
Copyright (c) 2021-2022 Alexei Sibidanov; ported to Rust in 2025 by Trevor Gross.
Its [MIT notice](licenses/core-math-MIT.txt) accompanies the Goose translation.
The FdLibm cube-root kernel remains separately translated from the pinned Horizon
source. Both are checked by raw-bit fixtures; future Rust toolchain changes can
alter the platform entry-point behavior and must be compared again.

[The port ledger](port-ledger.json) records symbol mappings, deviations and
verification evidence. The scope is Minecraft Java 26.3. Foundations and this
protocol subset do not yet constitute a playable game client.
The original voxel sandbox and independent VarInt primitive are separate work.

## Initial source review

The previous research examined the workspace and launcher manifests, account
flow (`launcher/src/account.rs`), protocol states and buffer primitives
(`crates/horizon-protocol/src/lib.rs`, `buf.rs`), and benchmarking methodology
(`docs/benchmark.md`). This is a partial review, not an exhaustive code audit.
The local checkout supplies the complete source for subsequent module mapping.

- Complete remaining `crates/horizon-protocol/src/chunk.rs` → `net/chunk.goose`,
  including heightmaps, block entity NBT, column buffers and lighting.
- Fifteen terrain/player/control clientbound variants from `packets/play.rs` → `net/play.goose`.
  The test oracle extracts selected original declarations and enum rows; it does
  not certify the omitted player/entity/inventory/play systems.
- `session263::receive_terrain` is a bounded diagnostic adaptation, not a port of
  the full client event/game loop. Batch throughput replies retain the reference
  `game/remote.rs` value of 64.0 chunks/tick.

- `crates/horizon-client/src/player_sync.rs` → `net/player_sync.goose` (movement
  selection and teleport velocity). The pose/acknowledgement portion comes from
  `game/remote.rs::remote_teleport`, without claiming its camera/model/UI behavior.
- `geometry/trig.rs` sine table and `geometry/mod.rs` X/Y rotations →
  `core/trig.goose`. Other geometry and inverse trigonometry remain pending.

- `registry.rs::Dynamic.dimension` layout fields and `DimensionType` defaults,
  validation and section count → `net/registry.goose`. Full registry APIs and
  clock/name semantics remain pending.
- `net/column.goose` is an original Goose ownership adapter connecting these
  compared dimension/section/light primitives; it is not a port of the renderer's
  resident world store.
