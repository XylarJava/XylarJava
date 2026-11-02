# XylarJava Client (Minecraft 1.8.9)

A standalone client for Minecraft **1.8.9**: vanilla + LaunchWrapper + Mixin, no Forge.

## Features

**75 mods and 73 options** (148 things you can switch or tune), grouped in the Right Shift menu
by category, with search. Click a card to switch a mod on or off; click its slider icon (or
right-click the card) for its options. *FPS boost preset* in the menu applies the fastest video
settings in one click and puts yours back when you click it again.

Nothing here gives an unfair advantage: no reach, no auto-clicker, no x-ray.

Also: the **XYLARJAVA title screen** (drawn with the game's own font), a **HUD editor** (drag
elements anywhere, scroll to resize) and a shared **HUD style** (background, opacity, shadow,
brackets, colour).

### Performance (13)

- **Optimized Settings** - One click for the fastest video settings. *Options: Fast graphics, Clouds off, Smooth lighting off, Mipmaps off, Entity shadows off, Use VBOs, Unlimited FPS, no VSync, View bobbing off, Minimal particles, Render distance (0 = keep).*
- **Entity Culling** - Don't draw entities that are far away. *Options: Render distance, Also hide far players, Hide armour stands, Hide far item frames, Hide arrows on the ground, Hide XP orbs.*
- **Block Entity Culling** - Don't draw chests and signs that are far away. *Options: Render distance.*
- **Particle Limit** - Caps how many particles can exist. *Options: Max particles.*
- **No Block Particles** - Less particles when mining. *Options: No block break particles, No block hit particles.*
- **No Weather Effects** - Skip rain and snow rendering. *Options: Hide rain and snow, No rain splashes.*
- **Background FPS Limit** - Lower FPS while you're tabbed out. *Options: FPS in background.*
- **Compact Dropped Items** - Draw item stacks on the ground as one item.
- **No Enchant Glint** - Skip the enchantment shimmer. *Options: On items, On armour.*
- **No Sky** - Don't draw the sky, sun and stars.
- **No Fog** - Remove distance fog (not underwater).
- **Static Textures** - Stop animating water, lava and fire.
- **Memory Cleaner** - Free unused memory now and then. *Options: Every.*

### HUD (26)

- **HUD Style** - Background, shadow and colours of the HUD. *Options: Background, Background opacity, Text shadow, Brackets, Text colour.*
- **FPS** - Frames per second.
- **CPS** - Clicks per second (left | right). *Options: Show right click.*
- **Coordinates** - Your position and facing direction. *Options: Show facing.*
- **Keystrokes** - Shows WASD and mouse clicks. *Options: Show mouse buttons, Show space bar, Pressed colour.*
- **Armor** - Armour and held item durability. *Options: Show held item, Durability as.*
- **Potion Effects** - Your effects and how long they last. *Options: Show icons.*
- **Ping** - Your latency to the server.
- **Clock** - The real time. *Options: Format, Show seconds.*
- **Memory** - How much RAM the game is using. *Options: Show.*
- **Server Address** - The server you're playing on.
- **Direction** - Which way you're facing. *Options: Show degrees.*
- **Biome** - The biome you're standing in.
- **Speed** - How fast you're moving, in blocks per second.
- **Reach Display** - Distance of your last hit.
- **Combo Counter** - Hits in a row without getting hit.
- **Day Counter** - Days passed in this world.
- **Arrow Counter** - Arrows in your inventory.
- **Light Level** - Light level where you stand.
- **Session Time** - How long you've been playing.
- **Player Count** - Players online on the server.
- **Rotation** - Your yaw and pitch.
- **Chunk Position** - The chunk you're in.
- **Saturation** - Your hidden hunger saturation.
- **Held Item Durability** - Uses left on the item in your hand.
- **Sprint Status** - Shows when toggle sprint or sneak is on.

### Visual (22)

- **Zoom** - Hold C to zoom in. *Options: Zoom key, Mode, Zoom, Smooth camera, Scroll to adjust.*
- **Custom Crosshair** - Your own crosshair shape and colour. *Options: Style, Size, Gap, Thickness, Colour, Outline, Show in third person.*
- **Block Outline** - Colour and thickness of the block you look at. *Options: Colour, Opacity, Line width.*
- **Fullbright** - See in the dark.
- **Time Changer** - Pick your own time of day (only for you). *Options: Time.*
- **Clear Weather** - No rain or storms, only for you.
- **Low Fire** - Smaller fire overlay when you're burning. *Options: Lower by.*
- **No Hurt Camera** - No screen shake when you take damage.
- **Static FOV** - Speed and sprint don't change your FOV. *Options: Keep bow zoom.*
- **No Pumpkin Overlay** - See clearly with a pumpkin on.
- **No Vignette** - Remove the dark screen corners.
- **No Portal Overlay** - No purple swirl in nether portals.
- **Hide Burning Entities** - Don't draw flames on burning mobs.
- **Hitboxes** - Always show entity hitboxes.
- **1.7 Animations** - Swing while blocking, like 1.7. *Options: 1.7 block-hitting.*
- **Own Nametag** - See your own name in third person.
- **No Achievement Popups** - Hide the achievement toasts.
- **Clean Inventory** - Potion effects don't push your inventory aside.
- **Scoreboard** - Hide the sidebar or its red numbers. *Options: Hide scoreboard, Hide red numbers.*
- **Hide Titles** - No big titles in the middle of the screen.
- **Hide Boss Bar** - Remove the boss health bar.
- **No View Bobbing** - Keep the camera steady while walking.

### Chat (5)

- **Chat Timestamps** - The time next to every message. *Options: Format, Show seconds.*
- **Compact Chat** - Repeated messages stack as (x2), (x3)...
- **Longer Chat History** - Keep more than 100 chat lines. *Options: Lines kept.*
- **Name Hider** - Hide your name in chat, tab and nametags (for videos). *Options: Show as.*
- **Mention Sound** - A sound when someone writes your name.

### PvP (4)

- **Toggle Sprint** - Sprint without holding the key.
- **Toggle Sneak** - Tap sneak once to keep sneaking.
- **Numeric Ping** - Ping as a number in the player list.
- **Hit Particles** - More particles when you hit something. *Options: Always sharpness particles, Always crit particles, Multiplier.*

### Utility (5)

- **Borderless Fullscreen** - Instant alt-tab: F11 makes a borderless window.
- **Window Title** - Name the window XylarJava Client 1.8.9.
- **Low Armour Warning** - Tells you when armour is about to break. *Options: Warn below.*
- **XylarJava Title Screen** - Use the XylarJava title screen.
- **Focus Pause** - Choose whether alt-tab opens the pause menu. *Options: Pause when tabbed out.*

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
