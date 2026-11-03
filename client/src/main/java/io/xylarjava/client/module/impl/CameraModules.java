package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.KeySetting;
import io.xylarjava.client.setting.ModeSetting;
import net.minecraft.client.Minecraft;
import net.minecraft.entity.Entity;

/** Ways to look around without changing where you're facing. */
public final class CameraModules {

    private CameraModules() {}

    /**
     * Hold the key and the camera orbits in third person while the player keeps walking and aiming
     * where they were. The camera's rotation replaces the player's only where the view is set up
     * (camera, particles, name tags), so nothing is sent to the server.
     */
    public static class Freelook extends Module {
        private final KeySetting key = key("key", "Freelook key", 33);  // F
        private final ModeSetting mode = mode("mode", "Mode", "Hold", "Hold", "Toggle");
        private final ModeSetting view = mode("view", "View", "Behind", "Behind", "In front", "First person");
        private final BoolSetting invert = bool("invert", "Invert up and down", false);

        private boolean active;
        private boolean keyWasDown;
        private float yaw;
        private float pitch;
        private int savedView;

        public Freelook() {
            super("freelook", "Freelook", "Hold F to look around you without turning.", Category.PVP);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        @Override
        public void onTick(Minecraft mc) {
            boolean down = mc.currentScreen == null && key.isDown();
            boolean next = active;
            if (mode.is("Hold"))
                next = down;
            else if (down && !keyWasDown)
                next = !active;
            keyWasDown = down;
            if (mc.thePlayer == null || mc.getRenderViewEntity() == null)
                next = false;
            if (next != active)
                setActive(mc, next);
        }

        @Override
        protected void onDisable() {
            if (active)
                setActive(mc(), false);
        }

        private void setActive(Minecraft mc, boolean on) {
            if (on) {
                Entity view = mc.getRenderViewEntity();
                yaw = view.rotationYaw;
                pitch = view.rotationPitch;
                savedView = mc.gameSettings.thirdPersonView;
                mc.gameSettings.thirdPersonView = this.view.is("Behind") ? 1 : this.view.is("In front") ? 2 : 0;
            } else {
                mc.gameSettings.thirdPersonView = savedView;
            }
            active = on;
            mc.renderGlobal.setDisplayListEntitiesDirty();
        }

        public boolean isActive() {
            return active;
        }

        /** The mouse moved: turn the camera instead of the player. Returns false when Freelook isn't in use. */
        public boolean turn(float dx, float dy) {
            if (!active)
                return false;
            yaw += dx * 0.15f;
            pitch += (invert.on() ? dy : -dy) * 0.15f;
            pitch = Math.max(-90f, Math.min(90f, pitch));
            return true;
        }

        public float yaw(Entity entity, float value) {
            return active && entity == mc().getRenderViewEntity() ? yaw : value;
        }

        public float pitch(Entity entity, float value) {
            return active && entity == mc().getRenderViewEntity() ? pitch : value;
        }
    }

    /** Hold the key for a quick look at yourself from the front, let go to go back. */
    public static class Snaplook extends Module {
        private final KeySetting key = key("key", "Snaplook key", 47);  // V
        private final ModeSetting view = mode("view", "View", "In front", "In front", "Behind");

        private boolean active;
        private int savedView;

        public Snaplook() {
            super("snaplook", "Snaplook", "Hold V to see yourself from the front.", Category.PVP);
        }

        @Override
        public void onTick(Minecraft mc) {
            boolean down = mc.currentScreen == null && mc.thePlayer != null && key.isDown();
            if (down == active)
                return;
            if (down) {
                savedView = mc.gameSettings.thirdPersonView;
                mc.gameSettings.thirdPersonView = view.is("In front") ? 2 : 1;
            } else {
                mc.gameSettings.thirdPersonView = savedView;
            }
            active = down;
        }

        @Override
        protected void onDisable() {
            if (active)
                mc().gameSettings.thirdPersonView = savedView;
            active = false;
        }
    }
}
