package io.xylarjava.client.module.impl;

import java.text.SimpleDateFormat;
import java.util.Collection;
import java.util.Date;
import java.util.Locale;

import io.xylarjava.client.Modules;
import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.module.TextHudModule;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.util.ClickTracker;
import io.xylarjava.client.util.CombatTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.client.multiplayer.ServerData;
import net.minecraft.client.network.NetworkPlayerInfo;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.resources.I18n;
import net.minecraft.init.Items;
import net.minecraft.item.ItemStack;
import net.minecraft.potion.Potion;
import net.minecraft.potion.PotionEffect;
import net.minecraft.util.BlockPos;
import net.minecraft.util.MathHelper;
import net.minecraft.util.ResourceLocation;
import net.minecraft.world.chunk.Chunk;

/** The one-line HUD elements, plus potion effects. */
public final class HudModules {

    private HudModules() {}

    static EntityPlayerSP player() {
        return Minecraft.getMinecraft().thePlayer;
    }

    public static class Fps extends TextHudModule {
        public Fps() {
            super("fps", "FPS", "Frames per second.", 0.01f, 0.01f);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        @Override
        protected String text() {
            return Minecraft.getDebugFPS() + " FPS";
        }
    }

    public static class Cps extends TextHudModule {
        private final BoolSetting right = bool("right", "Show right click", true);

        public Cps() {
            super("cps", "CPS", "Clicks per second (left | right).", 0.01f, 0.06f);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        @Override
        protected String text() {
            return right.on() ? ClickTracker.leftCps() + " | " + ClickTracker.rightCps() + " CPS" : ClickTracker.leftCps() + " CPS";
        }
    }

    public static class Ping extends TextHudModule {
        public Ping() {
            super("ping", "Ping", "Your latency to the server.", 0.12f, 0.01f);
        }

        @Override
        protected String text() {
            Minecraft mc = mc();
            if (mc.isSingleplayer() || mc.getNetHandler() == null || mc.thePlayer == null)
                return "0 ms";
            NetworkPlayerInfo info = mc.getNetHandler().getPlayerInfo(mc.thePlayer.getUniqueID());
            return (info == null ? 0 : info.getResponseTime()) + " ms";
        }
    }

    public static class Clock extends TextHudModule {
        private final ModeSetting format = mode("format", "Format", "24 hour", "24 hour", "12 hour");
        private final BoolSetting seconds = bool("seconds", "Show seconds", false);

        public Clock() {
            super("clock", "Clock", "The real time.", 0.88f, 0.01f);
        }

        @Override
        protected String text() {
            String pattern = format.is("24 hour") ? (seconds.on() ? "HH:mm:ss" : "HH:mm") : (seconds.on() ? "h:mm:ss a" : "h:mm a");
            return new SimpleDateFormat(pattern, Locale.ENGLISH).format(new Date());
        }
    }

    public static class Memory extends TextHudModule {
        private final ModeSetting style = mode("style", "Show", "Percent", "Percent", "Megabytes", "Both");

        public Memory() {
            super("memory", "Memory", "How much RAM the game is using.", 0.88f, 0.06f);
        }

        @Override
        protected String text() {
            Runtime rt = Runtime.getRuntime();
            long used = (rt.totalMemory() - rt.freeMemory()) / 1048576L;
            long max = rt.maxMemory() / 1048576L;
            long percent = max > 0 ? used * 100L / max : 0L;
            if (style.is("Percent"))
                return "Mem " + percent + "%";
            if (style.is("Megabytes"))
                return used + " / " + max + " MB";
            return "Mem " + percent + "% " + used + "/" + max + " MB";
        }
    }

    public static class Server extends TextHudModule {
        public Server() {
            super("server", "Server Address", "The server you're playing on.", 0.88f, 0.11f);
        }

        @Override
        protected String text() {
            Minecraft mc = mc();
            if (mc.isSingleplayer())
                return "Singleplayer";
            ServerData data = mc.getCurrentServerData();
            return data == null ? "Offline" : data.serverIP;
        }
    }

    public static class Direction extends TextHudModule {
        private static final String[] NAMES = {"South", "South West", "West", "North West", "North", "North East", "East", "South East"};
        private final BoolSetting degrees = bool("degrees", "Show degrees", true);

        public Direction() {
            super("direction", "Direction", "Which way you're facing.", 0.45f, 0.01f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            if (p == null)
                return "North";
            float yaw = MathHelper.wrapAngleTo180_float(p.rotationYaw);
            int index = MathHelper.floor_double((yaw + 360.0 + 22.5) / 45.0) & 7;
            return degrees.on() ? NAMES[index] + " " + Math.round(yaw < 0 ? yaw + 360f : yaw) + "°" : NAMES[index];
        }
    }

    public static class Biome extends TextHudModule {
        public Biome() {
            super("biome", "Biome", "The biome you're standing in.", 0.88f, 0.16f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            if (p == null || mc().theWorld == null)
                return "Biome";
            return mc().theWorld.getBiomeGenForCoords(new BlockPos(p)).biomeName;
        }
    }

    public static class Speed extends TextHudModule {
        private double lastX;
        private double lastZ;
        private double speed;

        public Speed() {
            super("speed", "Speed", "How fast you're moving, in blocks per second.", 0.88f, 0.21f);
        }

        @Override
        public void onTick(Minecraft mc) {
            EntityPlayerSP p = mc.thePlayer;
            if (p == null)
                return;
            double dx = p.posX - lastX;
            double dz = p.posZ - lastZ;
            speed = Math.sqrt(dx * dx + dz * dz) * 20.0;
            if (speed > 100.0)
                speed = 0.0;  // teleported
            lastX = p.posX;
            lastZ = p.posZ;
        }

        @Override
        protected String text() {
            return String.format(Locale.ROOT, "%.2f m/s", speed);
        }
    }

    public static class Reach extends TextHudModule {
        public Reach() {
            super("reach", "Reach Display", "Distance of your last hit.", 0.45f, 0.06f);
        }

        @Override
        protected String text() {
            double reach = CombatTracker.lastReach();
            return reach <= 0 ? "Hasn't attacked" : String.format(Locale.ROOT, "%.2f blocks", reach);
        }
    }

    public static class Combo extends TextHudModule {
        public Combo() {
            super("combo", "Combo Counter", "Hits in a row without getting hit.", 0.45f, 0.11f);
        }

        @Override
        protected String text() {
            int combo = CombatTracker.combo();
            return combo == 1 ? "1 hit" : combo + " combo";
        }
    }

    public static class Day extends TextHudModule {
        public Day() {
            super("day", "Day Counter", "Days passed in this world.", 0.88f, 0.26f);
        }

        @Override
        protected String text() {
            return mc().theWorld == null ? "Day 0" : "Day " + (mc().theWorld.getWorldTime() / 24000L);
        }
    }

    public static class Arrows extends TextHudModule {
        public Arrows() {
            super("arrows", "Arrow Counter", "Arrows in your inventory.", 0.45f, 0.86f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            int count = 0;
            if (p != null) {
                for (ItemStack stack : p.inventory.mainInventory) {
                    if (stack != null && stack.getItem() == Items.arrow)
                        count += stack.stackSize;
                }
            }
            return count + (count == 1 ? " arrow" : " arrows");
        }
    }

    public static class Light extends TextHudModule {
        public Light() {
            super("light", "Light Level", "Light level where you stand.", 0.88f, 0.31f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            if (p == null || mc().theWorld == null)
                return "Light 0";
            return "Light " + mc().theWorld.getLight(new BlockPos(p));
        }
    }

    public static class Session extends TextHudModule {
        private final long started = System.currentTimeMillis();

        public Session() {
            super("session", "Session Time", "How long you've been playing.", 0.88f, 0.36f);
        }

        @Override
        protected String text() {
            long seconds = (System.currentTimeMillis() - started) / 1000L;
            long h = seconds / 3600L;
            long m = (seconds % 3600L) / 60L;
            long s = seconds % 60L;
            return h > 0 ? String.format(Locale.ROOT, "%d:%02d:%02d", h, m, s) : String.format(Locale.ROOT, "%d:%02d", m, s);
        }
    }

    public static class Players extends TextHudModule {
        public Players() {
            super("players", "Player Count", "Players online on the server.", 0.88f, 0.41f);
        }

        @Override
        protected String text() {
            if (mc().getNetHandler() == null)
                return "1 player";
            Collection<NetworkPlayerInfo> infos = mc().getNetHandler().getPlayerInfoMap();
            int count = infos.size();
            return count + (count == 1 ? " player" : " players");
        }
    }

    public static class Rotation extends TextHudModule {
        public Rotation() {
            super("rotation", "Rotation", "Your yaw and pitch.", 0.88f, 0.46f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            if (p == null)
                return "Yaw 0 Pitch 0";
            return String.format(Locale.ROOT, "Yaw %.1f Pitch %.1f", MathHelper.wrapAngleTo180_float(p.rotationYaw), p.rotationPitch);
        }
    }

    public static class ChunkInfo extends TextHudModule {
        public ChunkInfo() {
            super("chunk", "Chunk Position", "The chunk you're in.", 0.88f, 0.51f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            if (p == null || mc().theWorld == null)
                return "Chunk 0, 0";
            Chunk chunk = mc().theWorld.getChunkFromBlockCoords(new BlockPos(p));
            return "Chunk " + chunk.xPosition + ", " + chunk.zPosition;
        }
    }

    public static class Saturation extends TextHudModule {
        public Saturation() {
            super("saturation", "Saturation", "Your hidden hunger saturation.", 0.88f, 0.56f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            return p == null ? "Saturation 0" : String.format(Locale.ROOT, "Saturation %.1f", p.getFoodStats().getSaturationLevel());
        }
    }

    public static class HeldItem extends TextHudModule {
        public HeldItem() {
            super("helditem", "Held Item Durability", "Uses left on the item in your hand.", 0.45f, 0.81f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            ItemStack stack = p == null ? null : p.getHeldItem();
            if (stack == null)
                return "Empty hand";
            if (!stack.isItemStackDamageable())
                return stack.getDisplayName();
            return (stack.getMaxDamage() - stack.getItemDamage()) + " / " + stack.getMaxDamage();
        }
    }

    public static class SprintStatus extends TextHudModule {
        public SprintStatus() {
            super("sprintstatus", "Sprint Status", "Shows when toggle sprint or sneak is on.", 0.01f, 0.95f);
        }

        @Override
        protected String text() {
            EntityPlayerSP p = player();
            if (Modules.TOGGLE_SNEAK.isToggled())
                return "Sneaking (Toggled)";
            if (p != null && p.isSprinting())
                return Modules.TOGGLE_SPRINT.isEnabled() ? "Sprinting (Toggled)" : "Sprinting";
            return "Walking";
        }
    }

    /** Active potion effects with their icon and time left. */
    public static class Potions extends HudModule {
        private static final ResourceLocation INVENTORY = new ResourceLocation("textures/gui/container/inventory.png");
        private final BoolSetting icons = bool("icons", "Show icons", true);

        public Potions() {
            super("potions", "Potion Effects", "Your effects and how long they last.", 0.01f, 0.6f);
        }

        private int lines() {
            EntityPlayerSP p = player();
            return p == null ? 0 : p.getActivePotionEffects().size();
        }

        @Override
        public int width() {
            return 110;
        }

        @Override
        public int height() {
            return Math.max(1, lines()) * 20 + 2;
        }

        @Override
        protected void draw(boolean editing) {
            EntityPlayerSP p = player();
            drawBackground(width(), height());
            if (p == null || p.getActivePotionEffects().isEmpty()) {
                drawText(editing ? "Speed II  1:30" : "No effects", 4, 7, textColor());
                return;
            }
            int y = 2;
            for (PotionEffect effect : p.getActivePotionEffects()) {
                Potion potion = Potion.potionTypes[effect.getPotionID()];
                int x = 4;
                if (icons.on() && potion.hasStatusIcon()) {
                    GlStateManager.color(1f, 1f, 1f, 1f);
                    mc().getTextureManager().bindTexture(INVENTORY);
                    int index = potion.getStatusIconIndex();
                    net.minecraft.client.gui.Gui.drawModalRectWithCustomSizedTexture(2, y, index % 8 * 18, 198 + index / 8 * 18, 18, 18, 256f, 256f);
                    x = 23;
                }
                String name = I18n.format(potion.getName());
                if (effect.getAmplifier() > 0)
                    name += " " + roman(effect.getAmplifier() + 1);
                drawText(name, x, y + 1, textColor());
                drawText(Potion.getDurationString(effect), x, y + 10, Modules.HUD_STYLE.isEnabled() ? 0xFF8E94A8 : 0xFF8E94A8);
                y += 20;
            }
        }

        private static String roman(int n) {
            String[] r = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
            return n >= 1 && n <= 10 ? r[n - 1] : String.valueOf(n);
        }
    }
}
