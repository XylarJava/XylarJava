# Earlier independent voxel sandbox

This archive documents the separate prototype. It is not the Rust-to-Goose
Minecraft port. See [README.md](README.md) for the active project.

## Separate sandbox

Status of this sandbox: **compiled and tested on Gentoo x86_64**. This is the
beginning of a separate native engine. It does not yet implement the Minecraft
Java protocol, vanilla gameplay, saves, assets or MSA session joining. The green
launcher selector targets only Minecraft Java 26.3, with installation disabled
until a Minecraft-compatible native release is available.

## Implemented

- `world.goose`: deterministic 64 × 32 × 64 terrain, trees, block storage,
  exposed-face meshing in sixteen 16 × 32 × 16 chunks, dirty neighbor tracking.
- `player.goose`: first-person camera state, fixed 120 Hz movement, gravity,
  jumping, axis-separated collision with substeps, normalized diagonal movement,
  DDA block targeting, break/place validation and player overlap prevention.
- `client.goose`: event/render loop, WASD/mouse input, five creative block slots,
  pause/focus handling and repeatable hidden previews. This is a creative
  sandbox; the physics and procedural terrain are not vanilla implementations.
- `native/`: a scalar C ABI for SDL2 window/input/timing and OpenGL 2.1 drawing.
  GPU vertex buffers cache chunk geometry; only dirty chunks are uploaded.
  One bounded CPU scratch mesh is reused. No external textures/game files.
- `protocol.goose`: signed Java VarInt encoding/decoding with golden-vector tests.

The world, collision, ray traversal and interaction logic live in **Goose**, not
in the C platform bridge. The C graphics test uses its own small fixture and
must never be presented as a running Goose game.

## Build the client (Linux prototype)

Install the [aardappel Goose compiler](https://aardappel.github.io/goose/)
(not another project named Goose), a C compiler, SDL2 development files and
OpenGL development files. The current `client.build` links Linux libraries;
Windows/macOS packaging is still to be implemented.

From the repository root:

```sh
goose check goose/client.goose
goose build --debug -o build-goose/protocol-test goose/protocol_test.goose
./build-goose/protocol-test
goose build --debug -o build-goose/engine-test goose/engine_test.goose
./build-goose/engine-test
goose build --debug -o build-goose/xylar-goose-debug goose/client.goose
goose build -o build-goose/xylar-goose goose/client.goose
./build-goose/xylar-goose --seed 42
```

WASD walks, mouse looks, Space jumps, Shift sprints. Left click breaks a block;
right click places it. Keys 1–5 or the wheel select grass, dirt, stone, wood or
leaves. Escape releases/recaptures the mouse. Focus loss pauses simulation;
click resumes without breaking a block. Closing the window exits. There is
**no saving yet**. This does not open or modify Minecraft worlds.

```sh
./build-goose/xylar-goose --seed 42 --frames 60
```

`--frames` uses a hidden window and deterministic physics steps, writing
`goose-preview.ppm` in the working directory. `SDL_VIDEODRIVER=offscreen` can
run the graphics backend without a desktop when SDL/Mesa support it.

To build this separate sandbox with CMake, explicitly configure
`cmake -S goose -B build-goose -DGOOSE_BUILD_SANDBOX=ON` and build that directory.
The normal build excludes the sandbox and its SDL/OpenGL dependencies, building
the translated foundation tests and terminal exerciser instead. Without Goose,
only the optional sandbox's C fixtures can be built.
Pin the compiler revision before CI/release integration. Use optimized native
builds for timing; the TinyCC development runner is not a performance baseline.

## Verification in this workspace

The user installed the official **v2026.3 Linux x64** archive. Its SHA-256 is
`06ac30284338269d3996d457412bf4f2b47c1f9a5601de65b6317187fe78d5c9`;
`goose version` reports runtime `4d3da949e563`. The local command is a symlink
from `~/.local/bin/goose` to the extracted compiler in `~/Downloads/goose/`;
keep that extracted directory intact (it contains the standard library/runtime).

All `.goose` sources now parse and typecheck. Debug and optimized native clients
were built using GCC 16. The protocol golden vectors and headless engine tests
passed. The engine tests cover mesh seams, dirty propagation, generation, spawn,
landing/jumping/walls/ceilings, fall tunneling, diagonal speed, ray reach and
placement validation. The actual Goose client rendered 60 frames offscreen,
producing `build-goose/client-preview/goose-preview.ppm`.

On this multilib Gentoo installation the embedded TinyCC runner (`goose run`)
reads a 32-bit libc linker script and fails. Native `goose build` works. CMake
therefore compiles the test programs to native executables instead of using the
runner. To reproduce the prototype and foundation unit tests (for the
port-only suite, see [PORTING.md](PORTING.md)):

```sh
cmake -S goose -B build-goose -DGOOSE_BUILD_SANDBOX=ON -DGOOSE_TEST_GRAPHICS=ON \
  -DGOOSE_EXECUTABLE="$HOME/.local/bin/goose"
cmake --build build-goose -j 1
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  ctest --test-dir build-goose --output-on-failure
```

The client preview test runs the optimized prototype executable. Its 60-frame preview also
matched the debug client preview byte for byte on the same backend.
The test suite verifies a prototype, not vanilla compatibility or performance.

The C backend was compiled with GCC 16, `-Wall -Wextra -Werror`, then exercised
with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S goose -B build-goose-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DGOOSE_BUILD_SANDBOX=ON -DGOOSE_TEST_GRAPHICS=ON \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-goose-sanitize -j 2
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ASAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir build-goose-sanitize --output-on-failure
```

Both C tests passed: face orientation/coordinates/bounds/allocation reuse, and
actual offscreen OpenGL upload/drawing/mesh replacement/screenshot/cleanup.
The graphics test verifies visible fixture colors in the rendered image, not
only GL return codes. The renderer reported Mesa llvmpipe, OpenGL 4.6
compatibility. Leak detection is disabled for the driver-backed test; this is
not a leak audit or an FPS benchmark. No Java-vs-Goose comparison exists yet.

## Architecture and delivery gates

1. **Protocol**: framing, lengths/quotas, zlib, NBT, version-specific registries
   and packet IDs, pinned to 26.3. Test fixtures against
   each actual vanilla server; a shared label does not prove compatibility.
2. **Login**: reuse XylarJava's MSA refresh and entitlement checks. A future native
   client must perform the Minecraft session join and encrypted online login.
   Local identities may connect only when the server accepts offline login. Never
   downgrade an online login automatically. Never put access tokens in process
   arguments, logs or instance configuration; define a private session handoff.
3. **Assets**: use locally obtained, version-matched game resources. The native
   source tree must not contain Mojang jars, textures or extracted game data.
4. **World/rendering**: replace the finite sandbox's flat byte grid and basic
   colors with vanilla block-state palettes, model/texture assets, lighting,
   streaming and view-frustum culling. Current chunk mesh caching is a starting
   point; it does not establish Java compatibility or low-end performance.
5. **Gameplay**: validate movement/collision against vanilla and add inventory,
   entities, ticking, redstone and saves.
   Compare identical inputs against vanilla per subsystem and version. Use
   disposable copied worlds until save round trips are verified.
6. **Distribution**: only enable installation once real native builds exist,
   with platform manifests, download verification and a native launch task.
   Java mod jars cannot be advertised as compatible with a native rewrite.

The low-end recommendation is a delivery goal. To publish it, compare the same
world, camera path, resolution and render/simulation distance on the same low-end
machines. Record CPU/GPU/RAM/driver, native build and compiler revision, FPS,
p95/p99 frame times, peak/steady RSS and startup/chunk load times, with repeated
runs. No whole-client measurement currently establishes a speedup. The translated palette kernel is measured separately in [BENCHMARK.md](BENCHMARK.md).

## Research notes (2026-11-05)

The existing XylarJava `CustomPage` selects metadata-backed Java versions and
`VanillaCreationTask` creates Java instances. `MinecraftAccount::fillSession`
provides the selected account's token/profile; `ClaimAccount` holds the account
use lock. A native engine needs a different creation/launch pipeline rather than
a fabricated metadata loader ID. Current Java account behavior is unchanged.

Technical source provenance, review notes and required acknowledgements are
kept in [UPSTREAM.md](UPSTREAM.md). See the
[Goose CLI documentation](https://github.com/aardappel/goose/blob/master/docs/command_line.md)
for native builds and C interoperability.
