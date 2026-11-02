package io.xylarjava.client;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import io.xylarjava.client.module.Module;
import io.xylarjava.client.module.impl.ArmorModule;
import io.xylarjava.client.module.impl.ChatModules;
import io.xylarjava.client.module.impl.CoordinatesModule;
import io.xylarjava.client.module.impl.HudModules;
import io.xylarjava.client.module.impl.HudStyle;
import io.xylarjava.client.module.impl.KeystrokesModule;
import io.xylarjava.client.module.impl.PerformanceModules;
import io.xylarjava.client.module.impl.PvpModules;
import io.xylarjava.client.module.impl.ToggleSprintModule;
import io.xylarjava.client.module.impl.UtilityModules;
import io.xylarjava.client.module.impl.VisualModules;
import io.xylarjava.client.module.impl.ZoomModule;

/**
 * Every module, created once. The hooks read these fields directly, so a render-thread
 * check is a field read and a boolean, nothing more.
 */
public final class Modules {

    private Modules() {}

    // Performance
    public static final PerformanceModules.OptimizedSettings OPTIMIZED_SETTINGS = new PerformanceModules.OptimizedSettings();
    public static final PerformanceModules.EntityCulling ENTITY_CULLING = new PerformanceModules.EntityCulling();
    public static final PerformanceModules.TileEntityCulling TILE_ENTITY_CULLING = new PerformanceModules.TileEntityCulling();
    public static final PerformanceModules.ParticleLimit PARTICLE_LIMIT = new PerformanceModules.ParticleLimit();
    public static final PerformanceModules.BlockParticles BLOCK_PARTICLES = new PerformanceModules.BlockParticles();
    public static final PerformanceModules.WeatherEffects WEATHER_EFFECTS = new PerformanceModules.WeatherEffects();
    public static final PerformanceModules.UnfocusedFps UNFOCUSED_FPS = new PerformanceModules.UnfocusedFps();
    public static final PerformanceModules.CompactItems COMPACT_ITEMS = new PerformanceModules.CompactItems();
    public static final PerformanceModules.NoGlint NO_GLINT = new PerformanceModules.NoGlint();
    public static final PerformanceModules.NoSky NO_SKY = new PerformanceModules.NoSky();
    public static final PerformanceModules.NoFog NO_FOG = new PerformanceModules.NoFog();
    public static final PerformanceModules.StaticTextures STATIC_TEXTURES = new PerformanceModules.StaticTextures();
    public static final PerformanceModules.MemoryCleaner MEMORY_CLEANER = new PerformanceModules.MemoryCleaner();

    // HUD
    public static final HudStyle HUD_STYLE = new HudStyle();
    public static final HudModules.Fps FPS = new HudModules.Fps();
    public static final HudModules.Cps CPS = new HudModules.Cps();
    public static final CoordinatesModule COORDINATES = new CoordinatesModule();
    public static final KeystrokesModule KEYSTROKES = new KeystrokesModule();
    public static final ArmorModule ARMOR = new ArmorModule();
    public static final HudModules.Potions POTIONS = new HudModules.Potions();
    public static final HudModules.Ping PING = new HudModules.Ping();
    public static final HudModules.Clock CLOCK = new HudModules.Clock();
    public static final HudModules.Memory MEMORY = new HudModules.Memory();
    public static final HudModules.Server SERVER = new HudModules.Server();
    public static final HudModules.Direction DIRECTION = new HudModules.Direction();
    public static final HudModules.Biome BIOME = new HudModules.Biome();
    public static final HudModules.Speed SPEED = new HudModules.Speed();
    public static final HudModules.Reach REACH = new HudModules.Reach();
    public static final HudModules.Combo COMBO = new HudModules.Combo();
    public static final HudModules.Day DAY = new HudModules.Day();
    public static final HudModules.Arrows ARROWS = new HudModules.Arrows();
    public static final HudModules.Light LIGHT = new HudModules.Light();
    public static final HudModules.Session SESSION = new HudModules.Session();
    public static final HudModules.Players PLAYERS = new HudModules.Players();
    public static final HudModules.Rotation ROTATION = new HudModules.Rotation();
    public static final HudModules.ChunkInfo CHUNK = new HudModules.ChunkInfo();
    public static final HudModules.Saturation SATURATION = new HudModules.Saturation();
    public static final HudModules.HeldItem HELD_ITEM = new HudModules.HeldItem();
    public static final HudModules.SprintStatus SPRINT_STATUS = new HudModules.SprintStatus();

    // Visual
    public static final ZoomModule ZOOM = new ZoomModule();
    public static final VisualModules.Crosshair CROSSHAIR = new VisualModules.Crosshair();
    public static final VisualModules.BlockOutline BLOCK_OUTLINE = new VisualModules.BlockOutline();
    public static final VisualModules.Fullbright FULLBRIGHT = new VisualModules.Fullbright();
    public static final VisualModules.TimeChanger TIME_CHANGER = new VisualModules.TimeChanger();
    public static final VisualModules.ClearWeather CLEAR_WEATHER = new VisualModules.ClearWeather();
    public static final VisualModules.LowFire LOW_FIRE = new VisualModules.LowFire();
    public static final VisualModules.NoHurtCam NO_HURT_CAM = new VisualModules.NoHurtCam();
    public static final VisualModules.StaticFov STATIC_FOV = new VisualModules.StaticFov();
    public static final VisualModules.NoPumpkin NO_PUMPKIN = new VisualModules.NoPumpkin();
    public static final VisualModules.NoVignette NO_VIGNETTE = new VisualModules.NoVignette();
    public static final VisualModules.NoPortalOverlay NO_PORTAL = new VisualModules.NoPortalOverlay();
    public static final VisualModules.HideEntityFire HIDE_ENTITY_FIRE = new VisualModules.HideEntityFire();
    public static final VisualModules.Hitboxes HITBOXES = new VisualModules.Hitboxes();
    public static final VisualModules.OldAnimations OLD_ANIMATIONS = new VisualModules.OldAnimations();
    public static final VisualModules.OwnNametag OWN_NAMETAG = new VisualModules.OwnNametag();
    public static final VisualModules.NoAchievements NO_ACHIEVEMENTS = new VisualModules.NoAchievements();
    public static final VisualModules.CleanInventory CLEAN_INVENTORY = new VisualModules.CleanInventory();
    public static final VisualModules.Scoreboard SCOREBOARD = new VisualModules.Scoreboard();
    public static final VisualModules.HideTitles HIDE_TITLES = new VisualModules.HideTitles();
    public static final VisualModules.HideBossBar HIDE_BOSS_BAR = new VisualModules.HideBossBar();
    public static final VisualModules.NoBobbingHand NO_BOBBING = new VisualModules.NoBobbingHand();

    // Chat
    public static final ChatModules.Timestamps TIMESTAMPS = new ChatModules.Timestamps();
    public static final ChatModules.CompactChat COMPACT_CHAT = new ChatModules.CompactChat();
    public static final ChatModules.ChatHistory CHAT_HISTORY = new ChatModules.ChatHistory();
    public static final ChatModules.NameHider NAME_HIDER = new ChatModules.NameHider();
    public static final ChatModules.MentionPing MENTION_PING = new ChatModules.MentionPing();

    // PvP
    public static final ToggleSprintModule TOGGLE_SPRINT = new ToggleSprintModule();
    public static final PvpModules.ToggleSneak TOGGLE_SNEAK = new PvpModules.ToggleSneak();
    public static final PvpModules.TabPing TAB_PING = new PvpModules.TabPing();
    public static final PvpModules.HitParticles HIT_PARTICLES = new PvpModules.HitParticles();

    // Utility
    public static final UtilityModules.BorderlessFullscreen BORDERLESS = new UtilityModules.BorderlessFullscreen();
    public static final UtilityModules.WindowTitle WINDOW_TITLE = new UtilityModules.WindowTitle();
    public static final UtilityModules.ArmorWarning ARMOR_WARNING = new UtilityModules.ArmorWarning();
    public static final UtilityModules.CustomMainMenu CUSTOM_MAIN_MENU = new UtilityModules.CustomMainMenu();
    public static final UtilityModules.PauseOnLostFocus PAUSE_ON_LOST_FOCUS = new UtilityModules.PauseOnLostFocus();

    private static final List<Module> ALL;

    static {
        List<Module> all = new ArrayList<Module>();
        Collections.addAll(all, OPTIMIZED_SETTINGS, ENTITY_CULLING, TILE_ENTITY_CULLING, PARTICLE_LIMIT, BLOCK_PARTICLES, WEATHER_EFFECTS,
                UNFOCUSED_FPS, COMPACT_ITEMS, NO_GLINT, NO_SKY, NO_FOG, STATIC_TEXTURES, MEMORY_CLEANER);
        Collections.addAll(all, HUD_STYLE, FPS, CPS, COORDINATES, KEYSTROKES, ARMOR, POTIONS, PING, CLOCK, MEMORY, SERVER, DIRECTION, BIOME,
                SPEED, REACH, COMBO, DAY, ARROWS, LIGHT, SESSION, PLAYERS, ROTATION, CHUNK, SATURATION, HELD_ITEM, SPRINT_STATUS);
        Collections.addAll(all, ZOOM, CROSSHAIR, BLOCK_OUTLINE, FULLBRIGHT, TIME_CHANGER, CLEAR_WEATHER, LOW_FIRE, NO_HURT_CAM, STATIC_FOV,
                NO_PUMPKIN, NO_VIGNETTE, NO_PORTAL, HIDE_ENTITY_FIRE, HITBOXES, OLD_ANIMATIONS, OWN_NAMETAG, NO_ACHIEVEMENTS, CLEAN_INVENTORY,
                SCOREBOARD, HIDE_TITLES, HIDE_BOSS_BAR, NO_BOBBING);
        Collections.addAll(all, TIMESTAMPS, COMPACT_CHAT, CHAT_HISTORY, NAME_HIDER, MENTION_PING);
        Collections.addAll(all, TOGGLE_SPRINT, TOGGLE_SNEAK, TAB_PING, HIT_PARTICLES);
        Collections.addAll(all, BORDERLESS, WINDOW_TITLE, ARMOR_WARNING, CUSTOM_MAIN_MENU, PAUSE_ON_LOST_FOCUS);
        ALL = Collections.unmodifiableList(all);
    }

    public static List<Module> all() {
        return ALL;
    }
}
