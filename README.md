<p align="center">
  <img src="launcher/resources/assets/XylarJava.png" alt="XylarJava" width="160">
</p>

<h1 align="center">XylarJava</h1>

<p align="center">A free Minecraft: Java Edition launcher for Windows and Linux.</p>

## Features

- **Every version, every loader**: any release or snapshot, with Fabric, Forge, NeoForge or Quilt. Modpacks from Modrinth, CurseForge, FTB, ATLauncher and Technic.
- **Skins & capes**: pick a skin and a XylarJava cape, or upload your own. Other XylarJava players see them, on offline servers too.
- **Servers in one click**: host a server on your computer and share it through a playit.gg tunnel, with your addresses hidden until you ask.
- **Friends**: see who's online, invite friends, join their server and show what you're playing on Discord.
- **XylarJava Client 1.8.9**: a PvP client with 90+ mods and nothing that gives an unfair advantage ([client/](client/)).
- **Repair**: when a game doesn't start or crashes, XylarJava finds out why and offers the fix.

## Goose engine development

**Xylar Goose** is the native engine project, porting an existing Rust
implementation to Goose, targeting **Minecraft Java 26.3 only**. Packed storage,
resident palettes, scalar chunk coordinates, Java integer parsing and biome seed
hashing are translated and compared, along with seeded random streams (including
floating-point and Gaussian draws) and an x86-64 Linux math subset.
The verified protocol subset now includes
integer/string primitives, handshake/status/login packets, chunk sections and
frame/compression policy (using system zlib). A Linux network diagnostic now
integrates status and offline login through acknowledgement, with adapted
RSA/AES. Binary NBT and configuration codecs are now compared; the diagnostic
can acknowledge configuration and decode a first chunk with lighting. Full chunk
wire data, fifteen Play variants, player synchronization and dimension layouts are
compared. The first column binds to its dimension; a running world and gameplay remain pending. See
[the porting scope and status](goose/PORTING.md).
The instance dialog includes a green **Goose** selector for the planned native
engine. Installation stays disabled until the translated client is playable.
The separate earlier voxel sandbox does not count as port coverage. Native
palette-building measurements are recorded in [the benchmark report](goose/BENCHMARK.md);
whole-client performance and low-end suitability remain unmeasured. See
[the engine source](goose/README.md).

## Download

Get the latest build from the [Releases](../../releases) page or from [corp.xylarinc.workers.dev](https://corp.xylarinc.workers.dev/downloads).

> [!WARNING]
> Offline accounts **cannot** join online-mode servers such as Hypixel. Those servers need a Microsoft account that owns Minecraft.

## Building

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Community

Questions, bugs and new builds: the [Xylar Discord](https://discord.gg/CQ6Et3Kws7).

## License

XylarJava is free software under the [GNU GPL v3](LICENSE). It's built on the MultiMC family of open-source launchers; the full credits and the licenses of everything it includes are in [COPYING.md](COPYING.md).

The interface is inspired by the Dawn launcher ([dawn.gg](https://dawn.gg)).

XylarJava is not affiliated with or endorsed by Mojang Studios or Microsoft. Minecraft is a trademark of Mojang Studios.
