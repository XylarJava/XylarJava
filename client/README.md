# XylarJava Client (Minecraft 1.8.9)

A standalone client for Minecraft **1.8.9**: vanilla + LaunchWrapper + Mixin, no Forge.

## Features

- **Title screen**: big XYLARJAVA title drawn with the game's own font, Singleplayer /
  Multiplayer / Mods / Settings / Quit, over the vanilla panorama.
- **HUD**: FPS, CPS, keystrokes, coordinates, armour. Drag them anywhere and scroll to resize
  (Right Shift → *Edit HUD layout*).
- **Toggle Sprint** and **Zoom** (hold `C`).
- **Mod menu**: press **Right Shift** in game to switch features on and off.

Settings are saved in `<instance>/.minecraft/xylarjava/config.json`.

## Build

Needs **Java 8** (ForgeGradle 2.1 doesn't run on newer Java).

```sh
cd client
./gradlew build        # Windows: gradlew.bat build
```

The jar ends up in `build/libs/xylarjava-client-0.1.0.jar`.

## Install in XylarJava

1. Create an instance with **Minecraft 1.8.9** (no mod loader).
2. Open the instance folder and copy the jar to
   `libraries/xylarjava-client-0.1.0.jar` (create the `libraries` folder if needed).
3. Copy `instance/io.xylarjava.client.json` to the instance's `patches/` folder.
4. Open `mmc-pack.json` in the instance folder and add this entry at the end of the
   `"components"` list (after the `net.minecraft` entry):

   ```json
   { "cachedName": "XylarJava Client", "cachedVersion": "0.1.0", "uid": "io.xylarjava.client" }
   ```
5. Restart the launcher and press **Play**. In *Edit Instance → Version* the client now shows
   up as a component.

The component tells the launcher to start the game through LaunchWrapper with
`io.xylarjava.client.XylarTweaker`, which loads the client's mixins.
