# First playable Goose 26.3 milestone

The first target is a **minimal multiplayer client connected to an existing
local offline-mode 26.3 server**: join, see real server chunks, move with
collision, and place/break blocks with server updates. This keeps world
simulation on the server while the native client is brought up. It is not the
independent earlier voxel sandbox, and does not yet include single player.

A reliable delivery date cannot be inferred from fixture counts. Expect weeks
of sustained work for a limited first client, and many months or more for broad
client coverage, subject to Goose graphics/runtime constraints and integration
findings. This is a rough planning estimate, not a release commitment.

| Milestone | Observable completion condition | Current evidence |
| --- | --- | --- |
| Foundations | Compared storage, palettes, math and seeded random streams | Existing Rust/Goose parity reports |
| Network/login | Status and login including compression/encryption | Native diagnostic and stream tests through login/configuration; actual TCP/server test remains required outside the socket-restricted sandbox |
| Configuration | Consume registries/tags/NBT, negotiate supported packs and client information, acknowledge configuration | Binary NBT and all configuration codecs compared; stream diagnostic collects data and acknowledges configuration. Dimension layout and biome palette width are now interpreted; full registry/resource semantics remain pending |
| Enter play | Decode player login/position, acknowledge teleports, keep connection alive | Login/respawn codecs and teleport pose/velocity/acknowledgements compared and stream-tested. Respawn world replacement, loading tracker and real-server verification pending |
| Real world data | Decode chunk/light/block updates into resident world data | Complete chunk/light wire codecs compared; first chunk received in stream integration. First column binds to the server dimension height, light sections and block/biome queries. Multi-column world storage and updates remain pending |
| Native view | Load prepared 26.3 resources and render real chunks with input | Renderer/resource pipeline port pending; sandbox graphics do not establish completion |
| Basic interaction | Server-backed movement/collision and block placement/breaking | Movement packet selection translated/compared; input/ticks, collision and block interaction pending |
| Distribution | Launcher installs/starts the native build; isolated end-to-end test | Selector exists, installation deliberately unavailable |

After this first milestone: Microsoft session joining, inventory/UI, entities,
audio, broader block behavior, performance work and the integrated single-player
server. These require their own acceptance tests; a protocol acknowledgement or
an increasing fixture count does not make the client playable.
