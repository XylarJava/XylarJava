package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.client.settings.KeyBinding;
import net.minecraft.entity.Entity;
import net.minecraft.util.EnumParticleTypes;

/** Fair PvP quality of life: nothing here gives an unfair advantage. */
public final class PvpModules {

    private PvpModules() {}

    /** Tap sneak to stay sneaking; tap again to stand up. */
    public static class ToggleSneak extends Module {
        private boolean toggled;
        private boolean wasDown;

        public ToggleSneak() {
            super("togglesneak", "Toggle Sneak", "Tap sneak once to keep sneaking.", Category.PVP);
        }

        @Override
        public void onTick(Minecraft mc) {
            KeyBinding sneak = mc.gameSettings.keyBindSneak;
            boolean down = sneak.getKeyCode() > 0 && org.lwjgl.input.Keyboard.isKeyDown(sneak.getKeyCode());
            if (mc.currentScreen == null && down && !wasDown)
                toggled = !toggled;
            wasDown = down;
            if (toggled && mc.currentScreen == null)
                KeyBinding.setKeyBindState(sneak.getKeyCode(), true);
        }

        @Override
        protected void onDisable() {
            if (toggled) {
                toggled = false;
                KeyBinding.setKeyBindState(mc().gameSettings.keyBindSneak.getKeyCode(), false);
            }
        }

        public boolean isToggled() {
            return isEnabled() && toggled;
        }
    }

    public static class TabPing extends Module {
        public TabPing() {
            super("tabping", "Numeric Ping", "Ping as a number in the player list.", Category.PVP);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }
    }

    /** Extra crit/sharpness particles on every hit, like Lunar's "always sharpness". */
    public static class HitParticles extends Module {
        private final BoolSetting sharpness = bool("sharpness", "Always sharpness particles", true);
        private final BoolSetting crits = bool("crits", "Always crit particles", false);
        private final NumberSetting multiplier = number("multiplier", "Multiplier", 1f, 1f, 5f, 1f, "x");

        public HitParticles() {
            super("hitparticles", "Hit Particles", "More particles when you hit something.", Category.PVP);
        }

        public void onAttack(EntityPlayerSP player, Entity target) {
            Minecraft mc = mc();
            for (int i = 0; i < multiplier.intValue(); i++) {
                if (sharpness.on())
                    mc.effectRenderer.emitParticleAtEntity(target, EnumParticleTypes.CRIT_MAGIC);
                if (crits.on())
                    mc.effectRenderer.emitParticleAtEntity(target, EnumParticleTypes.CRIT);
            }
        }
    }
}
