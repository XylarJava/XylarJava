package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ColorSetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.gui.GuiChat;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.entity.item.EntityTNTPrimed;

/** Visual effects: shaders, colours and how things in the world are drawn. */
public final class EffectModules {

    private EffectModules() {}

    /** Blurs the world behind menus and inventories. The blur fades in when a menu opens. */
    public static class MenuBlur extends Module {
        private final NumberSetting strength = number("strength", "Strength", 8f, 2f, 20f, 1f, "");
        private final NumberSetting fade = number("fade", "Fade in", 200f, 0f, 600f, 50f, " ms");
        private final BoolSetting chat = bool("chat", "Also behind chat", false);

        public MenuBlur() {
            super("menublur", "Menu Blur", "Blur the world behind menus.", Category.VISUAL);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        public boolean wants(GuiScreen screen) {
            return isEnabled() && screen != null && (chat.on() || !(screen instanceof GuiChat));
        }

        /** Blur radius @p millis after the menu opened. */
        public float radius(long millis) {
            float t = fade.value() <= 0f ? 1f : Math.min(1f, millis / fade.value());
            t = 1f - (1f - t) * (1f - t);  // ease out
            return strength.value() * t;
        }
    }

    /** Frames blend into the next one, which smooths fast camera turns. */
    public static class MotionBlur extends Module {
        private final NumberSetting amount = number("amount", "Amount", 50f, 10f, 90f, 5f, "%");

        public MotionBlur() {
            super("motionblur", "Motion Blur", "Smooth trails when you turn quickly.", Category.VISUAL);
        }

        public float blend() {
            return amount.value() / 100f;
        }
    }

    /** Colour of the enchantment shine on items and armour. */
    public static class GlintColorizer extends Module {
        private final ColorSetting color = color("color", "Colour", 0xFF3CC8FF);
        private final BoolSetting rainbow = bool("rainbow", "Rainbow", false);

        public GlintColorizer() {
            super("glintcolor", "Glint Colorizer", "Pick the colour of the enchantment shine.", Category.VISUAL);
        }

        public int argb() {
            if (rainbow.on())
                return 0xFF000000 | (java.awt.Color.HSBtoRGB((System.currentTimeMillis() % 4000L) / 4000f, 0.7f, 1f) & 0xFFFFFF);
            return 0xFF000000 | color.rgb();
        }
    }

    /** Colour and strength of the flash on mobs and players that get hit. */
    public static class HitColor extends Module {
        private final ColorSetting color = color("color", "Colour", 0xFFFF4F5E);
        private final NumberSetting opacity = number("opacity", "Strength", 30f, 0f, 100f, 5f, "%");

        public HitColor() {
            super("hitcolor", "Hit Color", "Colour of the hurt flash on players and mobs.", Category.PVP);
        }

        public float red() {
            return ((color.argb() >> 16) & 255) / 255f;
        }

        public float green() {
            return ((color.argb() >> 8) & 255) / 255f;
        }

        public float blue() {
            return (color.argb() & 255) / 255f;
        }

        public float alpha() {
            return opacity.value() / 100f;
        }
    }

    /** Seconds left above lit TNT, going from green to red. */
    public static class TntCountdown extends Module {
        private final ModeSetting format = mode("format", "Show", "Seconds", "Seconds", "Ticks");

        public TntCountdown() {
            super("tntcountdown", "TNT Countdown", "Time left before lit TNT explodes.", Category.VISUAL);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        public String label(EntityTNTPrimed tnt, float partialTicks) {
            float ticks = Math.max(0f, tnt.fuse - partialTicks);
            String color = ticks > 40f ? "§a" : ticks > 20f ? "§e" : ticks > 10f ? "§6" : "§c";
            if (format.is("Ticks"))
                return color + Math.round(ticks);
            return color + String.format(java.util.Locale.ROOT, "%.2f", ticks / 20f);
        }
    }

    /** Dropped items lie flat on the ground and tumble while falling. */
    public static class ItemPhysics extends Module {
        private final BoolSetting tumble = bool("tumble", "Spin while falling", true);

        public ItemPhysics() {
            super("itemphysics", "Item Physics", "Dropped items lie flat on the ground.", Category.VISUAL);
        }

        public boolean tumble() {
            return tumble.on();
        }
    }

    /** Fog colour and distance, outside water and lava. */
    public static class FogCustomizer extends Module {
        private final NumberSetting distance = number("distance", "Distance", 100f, 25f, 300f, 25f, "%");
        private final BoolSetting tint = bool("tint", "Tint the fog", false);
        private final ColorSetting color = color("color", "Tint colour", 0xFF6B8CFF);
        private final NumberSetting tintStrength = number("tintstrength", "Tint strength", 50f, 10f, 100f, 10f, "%");

        public FogCustomizer() {
            super("fogcustomizer", "Fog Customizer", "Change how far away the fog starts and its colour.", Category.VISUAL);
        }

        public float distance() {
            return distance.value() / 100f;
        }

        /** Mixes the tint into a vanilla fog colour; returns null when there's no tint. */
        public float[] tint(float red, float green, float blue) {
            if (!tint.on())
                return null;
            float t = tintStrength.value() / 100f;
            int c = color.argb();
            return new float[] {
                red + (((c >> 16) & 255) / 255f - red) * t,
                green + (((c >> 8) & 255) / 255f - green) * t,
                blue + ((c & 255) / 255f - blue) * t,
            };
        }
    }
}
