package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ColorSetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.ScaledResolution;

/** How the game looks. */
public final class VisualModules {

    private VisualModules() {}

    public static class Crosshair extends Module {
        private final ModeSetting style = mode("style", "Style", "Cross", "Cross", "Cross + dot", "Dot", "Circle", "T");
        private final NumberSetting size = number("size", "Size", 4f, 1f, 12f, 1f, " px");
        private final NumberSetting gap = number("gap", "Gap", 1f, 0f, 8f, 1f, " px");
        private final NumberSetting thickness = number("thickness", "Thickness", 1f, 1f, 4f, 1f, " px");
        private final ColorSetting color = color("color", "Colour", 0xFFFFFFFF);
        private final BoolSetting outline = bool("outline", "Outline", true);
        private final BoolSetting thirdPerson = bool("thirdperson", "Show in third person", false);

        public Crosshair() {
            super("crosshair", "Custom Crosshair", "Your own crosshair shape and colour.", Category.VISUAL);
        }

        public void draw(Minecraft mc, ScaledResolution res) {
            if (mc.gameSettings.thirdPersonView != 0 && !thirdPerson.on())
                return;
            int cx = res.getScaledWidth() / 2;
            int cy = res.getScaledHeight() / 2;
            int s = size.intValue();
            int g = gap.intValue();
            int t = thickness.intValue();
            int c = color.argb();
            String st = style.get();
            if (st.equals("Dot")) {
                bar(cx - t / 2, cy - t / 2, Math.max(2, t), Math.max(2, t), c);
            } else if (st.equals("Circle")) {
                int r = s + g;
                for (int i = 0; i < 32; i++) {
                    double a = Math.PI * 2 * i / 32;
                    int px = cx + (int) Math.round(Math.cos(a) * r);
                    int py = cy + (int) Math.round(Math.sin(a) * r);
                    bar(px, py, t, t, c);
                }
            } else {
                int half = t / 2;
                bar(cx - g - s, cy - half, s, t, c);          // left
                bar(cx + g + 1, cy - half, s, t, c);          // right
                bar(cx - half, cy + g + 1, t, s, c);          // bottom
                if (!st.equals("T"))
                    bar(cx - half, cy - g - s, t, s, c);      // top
                if (st.equals("Cross + dot"))
                    bar(cx - half, cy - half, t, t, c);
            }
        }

        private void bar(int x, int y, int w, int h, int color) {
            if (outline.on())
                Draw.rect(x - 1, y - 1, w + 2, h + 2, 0xAA000000);
            Draw.rect(x, y, w, h, color);
        }
    }

    public static class BlockOutline extends Module {
        public final ColorSetting color = color("color", "Colour", 0xFFFF9A3C);
        public final NumberSetting opacity = number("opacity", "Opacity", 80f, 10f, 100f, 5f, "%");
        public final NumberSetting width = number("width", "Line width", 2f, 1f, 6f, 0.5f, " px");

        public BlockOutline() {
            super("blockoutline", "Block Outline", "Colour and thickness of the block you look at.", Category.VISUAL);
        }
    }

    public static class Fullbright extends Module {
        public Fullbright() {
            super("fullbright", "Fullbright", "See in the dark.", Category.VISUAL);
        }

        @Override
        public void onTick(Minecraft mc) {
            if (mc.gameSettings.gammaSetting < 100f) {
                if (!state.settings.containsKey("orig.gamma"))
                    state.settings.put("orig.gamma", String.valueOf(mc.gameSettings.gammaSetting));
                mc.gameSettings.gammaSetting = 100f;
            }
        }

        @Override
        protected void onDisable() {
            String saved = state.settings.remove("orig.gamma");
            float gamma = 1f;
            try {
                if (saved != null)
                    gamma = Float.parseFloat(saved);
            } catch (NumberFormatException ignored) {
            }
            mc().gameSettings.gammaSetting = Math.min(1f, gamma);
        }
    }

    public static class TimeChanger extends Module {
        private final ModeSetting time = mode("time", "Time", "Day", "Sunrise", "Day", "Sunset", "Night", "Midnight");

        public TimeChanger() {
            super("timechanger", "Time Changer", "Pick your own time of day (only for you).", Category.VISUAL);
        }

        @Override
        public void onTick(Minecraft mc) {
            if (mc.theWorld != null)
                mc.theWorld.setWorldTime(ticks());
        }

        private long ticks() {
            String t = time.get();
            if (t.equals("Sunrise"))
                return 23000L;
            if (t.equals("Sunset"))
                return 12500L;
            if (t.equals("Night"))
                return 15000L;
            if (t.equals("Midnight"))
                return 18000L;
            return 6000L;
        }
    }

    public static class ClearWeather extends Module {
        public ClearWeather() {
            super("clearweather", "Clear Weather", "No rain or storms, only for you.", Category.VISUAL);
        }
    }

    public static class LowFire extends Module {
        private final NumberSetting height = number("height", "Lower by", 0.3f, 0.1f, 0.6f, 0.05f, "");

        public LowFire() {
            super("lowfire", "Low Fire", "Smaller fire overlay when you're burning.", Category.VISUAL);
        }

        public float offset() {
            return height.value();
        }
    }

    public static class NoHurtCam extends Module {
        public NoHurtCam() {
            super("nohurtcam", "No Hurt Camera", "No screen shake when you take damage.", Category.VISUAL);
        }
    }

    public static class StaticFov extends Module {
        private final BoolSetting bow = bool("bow", "Keep bow zoom", true);

        public StaticFov() {
            super("staticfov", "Static FOV", "Speed and sprint don't change your FOV.", Category.VISUAL);
        }

        public boolean keepBow() {
            return bow.on();
        }
    }

    public static class NoPumpkin extends Module {
        public NoPumpkin() {
            super("nopumpkin", "No Pumpkin Overlay", "See clearly with a pumpkin on.", Category.VISUAL);
        }
    }

    public static class NoVignette extends Module {
        public NoVignette() {
            super("novignette", "No Vignette", "Remove the dark screen corners.", Category.VISUAL);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }
    }

    public static class NoPortalOverlay extends Module {
        public NoPortalOverlay() {
            super("noportal", "No Portal Overlay", "No purple swirl in nether portals.", Category.VISUAL);
        }
    }

    public static class HideEntityFire extends Module {
        public HideEntityFire() {
            super("hideentityfire", "Hide Burning Entities", "Don't draw flames on burning mobs.", Category.VISUAL);
        }
    }

    public static class Hitboxes extends Module {
        public Hitboxes() {
            super("hitboxes", "Hitboxes", "Always show entity hitboxes.", Category.VISUAL);
        }

        @Override
        public void onTick(Minecraft mc) {
            if (!mc.getRenderManager().isDebugBoundingBox())
                mc.getRenderManager().setDebugBoundingBox(true);
        }

        @Override
        protected void onDisable() {
            mc().getRenderManager().setDebugBoundingBox(false);
        }
    }

    public static class OldAnimations extends Module {
        public final BoolSetting blockHit = bool("blockhit", "1.7 block-hitting", true);

        public OldAnimations() {
            super("oldanimations", "1.7 Animations", "Swing while blocking, like 1.7.", Category.VISUAL);
        }
    }

    public static class OwnNametag extends Module {
        public OwnNametag() {
            super("ownnametag", "Own Nametag", "See your own name in third person.", Category.VISUAL);
        }
    }

    public static class NoAchievements extends Module {
        public NoAchievements() {
            super("noachievements", "No Achievement Popups", "Hide the achievement toasts.", Category.VISUAL);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }
    }

    public static class CleanInventory extends Module {
        public CleanInventory() {
            super("cleaninventory", "Clean Inventory", "Potion effects don't push your inventory aside.", Category.VISUAL);
        }
    }

    public static class Scoreboard extends Module {
        public final BoolSetting hide = bool("hide", "Hide scoreboard", false);
        public final BoolSetting numbers = bool("numbers", "Hide red numbers", true);

        public Scoreboard() {
            super("scoreboard", "Scoreboard", "Hide the sidebar or its red numbers.", Category.VISUAL);
        }
    }

    public static class HideTitles extends Module {
        public HideTitles() {
            super("hidetitles", "Hide Titles", "No big titles in the middle of the screen.", Category.VISUAL);
        }
    }

    public static class HideBossBar extends Module {
        public HideBossBar() {
            super("hidebossbar", "Hide Boss Bar", "Remove the boss health bar.", Category.VISUAL);
        }
    }

    public static class NoBobbingHand extends Module {
        public NoBobbingHand() {
            super("nobobbing", "No View Bobbing", "Keep the camera steady while walking.", Category.VISUAL);
        }

        @Override
        public void onTick(Minecraft mc) {
            if (mc.gameSettings.viewBobbing) {
                state.settings.put("orig.bobbing", "true");
                mc.gameSettings.viewBobbing = false;
            }
        }

        @Override
        protected void onDisable() {
            if ("true".equals(state.settings.remove("orig.bobbing")))
                mc().gameSettings.viewBobbing = true;
        }
    }
}
