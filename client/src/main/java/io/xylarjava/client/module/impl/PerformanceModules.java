package io.xylarjava.client.module.impl;

import java.util.Map;

import org.lwjgl.opengl.Display;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.Minecraft;
import net.minecraft.client.settings.GameSettings;
import net.minecraft.entity.Entity;
import net.minecraft.entity.item.EntityArmorStand;
import net.minecraft.entity.item.EntityItemFrame;
import net.minecraft.entity.item.EntityXPOrb;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.entity.projectile.EntityArrow;
import net.minecraft.tileentity.TileEntity;

/** Things that buy frames. */
public final class PerformanceModules {

    private PerformanceModules() {}

    /**
     * Switches the game's own video settings to the fastest values, remembering yours so switching
     * the module off puts them back.
     */
    public static class OptimizedSettings extends Module {
        private final BoolSetting fastGraphics = bool("fast", "Fast graphics", true);
        private final BoolSetting noClouds = bool("clouds", "Clouds off", true);
        private final BoolSetting noSmoothLighting = bool("smoothlighting", "Smooth lighting off", true);
        private final BoolSetting noMipmaps = bool("mipmaps", "Mipmaps off", true);
        private final BoolSetting noShadows = bool("shadows", "Entity shadows off", true);
        private final BoolSetting vbo = bool("vbo", "Use VBOs", true);
        private final BoolSetting unlimited = bool("unlimited", "Unlimited FPS, no VSync", true);
        private final BoolSetting noBobbing = bool("bobbing", "View bobbing off", false);
        private final BoolSetting fewParticles = bool("particles", "Minimal particles", false);
        private final NumberSetting renderDistance = number("distance", "Render distance (0 = keep)", 0f, 0f, 16f, 1f, " chunks");

        public OptimizedSettings() {
            super("optimizedsettings", "Optimized Settings", "One click for the fastest video settings.", Category.PERFORMANCE);
        }

        @Override
        protected void onEnable() {
            Minecraft mc = mc();
            GameSettings gs = mc.gameSettings;
            Map<String, String> saved = state.settings;
            if (!saved.containsKey("orig.fancy"))
                rememberCurrent(gs, saved);


            boolean reloadRenderers = false;
            boolean reloadTextures = false;
            if (fastGraphics.on() && gs.fancyGraphics) {
                gs.fancyGraphics = false;
                reloadRenderers = true;
            }
            if (noClouds.on())
                gs.clouds = 0;
            if (noSmoothLighting.on() && gs.ambientOcclusion != 0) {
                gs.ambientOcclusion = 0;
                reloadRenderers = true;
            }
            if (noMipmaps.on() && gs.mipmapLevels != 0) {
                gs.mipmapLevels = 0;
                reloadTextures = true;
            }
            if (noShadows.on())
                gs.entityShadows = false;
            if (vbo.on() && !gs.useVbo) {
                gs.useVbo = true;
                reloadRenderers = true;
            }
            if (unlimited.on()) {
                gs.limitFramerate = (int) GameSettings.Options.FRAMERATE_LIMIT.getValueMax();
                gs.enableVsync = false;
                Display.setVSyncEnabled(false);
            }
            if (noBobbing.on())
                gs.viewBobbing = false;
            if (fewParticles.on())
                gs.particleSetting = 2;
            if (renderDistance.intValue() > 0 && gs.renderDistanceChunks > renderDistance.intValue()) {
                gs.renderDistanceChunks = renderDistance.intValue();
                reloadRenderers = true;
            }
            apply(mc, reloadRenderers, reloadTextures);
        }

        @Override
        protected void onDisable() {
            Minecraft mc = mc();
            GameSettings gs = mc.gameSettings;
            Map<String, String> saved = state.settings;
            if (!saved.containsKey("orig.fancy"))
                return;
            boolean oldFancy = gs.fancyGraphics;
            int oldAo = gs.ambientOcclusion;
            int oldMipmaps = gs.mipmapLevels;
            boolean oldVbo = gs.useVbo;
            int oldDistance = gs.renderDistanceChunks;
            gs.fancyGraphics = Boolean.parseBoolean(saved.get("orig.fancy"));
            gs.clouds = parse(saved.get("orig.clouds"), gs.clouds);
            gs.ambientOcclusion = parse(saved.get("orig.ao"), gs.ambientOcclusion);
            gs.mipmapLevels = parse(saved.get("orig.mipmaps"), gs.mipmapLevels);
            gs.entityShadows = Boolean.parseBoolean(saved.get("orig.shadows"));
            gs.useVbo = Boolean.parseBoolean(saved.get("orig.vbo"));
            gs.limitFramerate = parse(saved.get("orig.fps"), gs.limitFramerate);
            gs.enableVsync = Boolean.parseBoolean(saved.get("orig.vsync"));
            Display.setVSyncEnabled(gs.enableVsync);
            gs.viewBobbing = Boolean.parseBoolean(saved.get("orig.bobbing"));
            gs.particleSetting = parse(saved.get("orig.particles"), gs.particleSetting);
            gs.renderDistanceChunks = parse(saved.get("orig.distance"), gs.renderDistanceChunks);
            for (String key : new String[] {"fancy", "clouds", "ao", "mipmaps", "shadows", "vbo", "fps", "vsync", "bobbing", "particles", "distance"})
                saved.remove("orig." + key);
            boolean reloadRenderers = oldFancy != gs.fancyGraphics || oldAo != gs.ambientOcclusion || oldVbo != gs.useVbo
                    || oldDistance != gs.renderDistanceChunks;
            apply(mc, reloadRenderers, oldMipmaps != gs.mipmapLevels);
        }

        private static void rememberCurrent(GameSettings gs, Map<String, String> saved) {
            saved.put("orig.fancy", String.valueOf(gs.fancyGraphics));
            saved.put("orig.clouds", String.valueOf(gs.clouds));
            saved.put("orig.ao", String.valueOf(gs.ambientOcclusion));
            saved.put("orig.mipmaps", String.valueOf(gs.mipmapLevels));
            saved.put("orig.shadows", String.valueOf(gs.entityShadows));
            saved.put("orig.vbo", String.valueOf(gs.useVbo));
            saved.put("orig.fps", String.valueOf(gs.limitFramerate));
            saved.put("orig.vsync", String.valueOf(gs.enableVsync));
            saved.put("orig.bobbing", String.valueOf(gs.viewBobbing));
            saved.put("orig.particles", String.valueOf(gs.particleSetting));
            saved.put("orig.distance", String.valueOf(gs.renderDistanceChunks));
        }

        private static void apply(Minecraft mc, boolean reloadRenderers, boolean reloadTextures) {
            if (reloadTextures) {
                mc.getTextureMapBlocks().setMipmapLevels(mc.gameSettings.mipmapLevels);
                mc.scheduleResourcesRefresh();
            }
            if (reloadRenderers && mc.renderGlobal != null)
                mc.renderGlobal.loadRenderers();
            mc.gameSettings.saveOptions();
        }

        private static int parse(String text, int fallback) {
            try {
                return Integer.parseInt(text);
            } catch (Exception e) {
                return fallback;
            }
        }
    }

    /** Skips entities that are far away (and a few that rarely matter). */
    public static class EntityCulling extends Module {
        private final NumberSetting distance = number("distance", "Render distance", 64f, 16f, 256f, 8f, " blocks");
        private final BoolSetting players = bool("players", "Also hide far players", false);
        private final BoolSetting armorStands = bool("armorstands", "Hide armour stands", false);
        private final BoolSetting itemFrames = bool("itemframes", "Hide far item frames", true);
        private final BoolSetting stuckArrows = bool("arrows", "Hide arrows on the ground", false);
        private final BoolSetting xpOrbs = bool("xp", "Hide XP orbs", false);

        public EntityCulling() {
            super("entityculling", "Entity Culling", "Don't draw entities that are far away.", Category.PERFORMANCE);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        /** False when the entity shouldn't be drawn this frame. */
        public boolean shouldRender(Entity entity, double camX, double camY, double camZ) {
            if (entity == mc().getRenderViewEntity())
                return true;
            if (armorStands.on() && entity instanceof EntityArmorStand)
                return false;
            if (xpOrbs.on() && entity instanceof EntityXPOrb)
                return false;
            if (stuckArrows.on() && entity instanceof EntityArrow && entity.motionX == 0.0 && entity.motionZ == 0.0)
                return false;
            if (entity instanceof EntityPlayer && !players.on())
                return true;
            double limit = distance.value();
            if (entity instanceof EntityItemFrame && itemFrames.on())
                limit = Math.min(limit, 32.0);
            double dx = entity.posX - camX;
            double dy = entity.posY - camY;
            double dz = entity.posZ - camZ;
            return dx * dx + dy * dy + dz * dz <= limit * limit;
        }
    }

    /** Chests, signs, banners and skulls stop drawing past a distance. */
    public static class TileEntityCulling extends Module {
        private final NumberSetting distance = number("distance", "Render distance", 48f, 8f, 128f, 8f, " blocks");

        public TileEntityCulling() {
            super("tileentityculling", "Block Entity Culling", "Don't draw chests and signs that are far away.", Category.PERFORMANCE);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        public boolean shouldRender(TileEntity tile) {
            Entity view = mc().getRenderViewEntity();
            if (view == null)
                return true;
            double limit = distance.value();
            return tile.getDistanceSq(view.posX, view.posY, view.posZ) <= limit * limit;
        }
    }

    public static class ParticleLimit extends Module {
        private final NumberSetting max = number("max", "Max particles", 1000f, 100f, 4000f, 100f, "");

        public ParticleLimit() {
            super("particlelimit", "Particle Limit", "Caps how many particles can exist.", Category.PERFORMANCE);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        public int max() {
            return max.intValue();
        }
    }

    public static class BlockParticles extends Module {
        public final BoolSetting breaking = bool("break", "No block break particles", true);
        public final BoolSetting hitting = bool("hit", "No block hit particles", true);

        public BlockParticles() {
            super("blockparticles", "No Block Particles", "Less particles when mining.", Category.PERFORMANCE);
        }
    }

    public static class WeatherEffects extends Module {
        public final BoolSetting rain = bool("rain", "Hide rain and snow", true);
        public final BoolSetting splashes = bool("splashes", "No rain splashes", true);

        public WeatherEffects() {
            super("weathereffects", "No Weather Effects", "Skip rain and snow rendering.", Category.PERFORMANCE);
        }
    }

    /** Lower FPS while the game window isn't focused, to save power and keep the PC cool. */
    public static class UnfocusedFps extends Module {
        private final NumberSetting fps = number("fps", "FPS in background", 15f, 5f, 60f, 5f, " FPS");

        public UnfocusedFps() {
            super("unfocusedfps", "Background FPS Limit", "Lower FPS while you're tabbed out.", Category.PERFORMANCE);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        public int limit(int current) {
            return Display.isActive() ? current : Math.min(current, fps.intValue());
        }
    }

    public static class CompactItems extends Module {
        public CompactItems() {
            super("compactitems", "Compact Dropped Items", "Draw item stacks on the ground as one item.", Category.PERFORMANCE);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }
    }

    public static class NoGlint extends Module {
        public final BoolSetting items = bool("items", "On items", true);
        public final BoolSetting armor = bool("armor", "On armour", true);

        public NoGlint() {
            super("noglint", "No Enchant Glint", "Skip the enchantment shimmer.", Category.PERFORMANCE);
        }
    }

    public static class NoSky extends Module {
        public NoSky() {
            super("nosky", "No Sky", "Don't draw the sky, sun and stars.", Category.PERFORMANCE);
        }
    }

    public static class NoFog extends Module {
        public NoFog() {
            super("nofog", "No Fog", "Remove distance fog (not underwater).", Category.PERFORMANCE);
        }
    }

    public static class StaticTextures extends Module {
        public StaticTextures() {
            super("statictextures", "Static Textures", "Stop animating water, lava and fire.", Category.PERFORMANCE);
        }
    }

    /** Frees memory right away instead of waiting for the garbage collector. */
    public static class MemoryCleaner extends Module {
        private final NumberSetting minutes = number("minutes", "Every", 5f, 1f, 30f, 1f, " min");
        private int ticks;

        public MemoryCleaner() {
            super("memorycleaner", "Memory Cleaner", "Free unused memory now and then.", Category.PERFORMANCE);
        }

        @Override
        public void onTick(Minecraft mc) {
            if (++ticks >= minutes.intValue() * 1200) {
                ticks = 0;
                System.gc();
            }
        }
    }
}
