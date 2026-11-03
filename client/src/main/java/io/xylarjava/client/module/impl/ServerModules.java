package io.xylarjava.client.module.impl;

import java.util.Locale;

import io.xylarjava.client.gui.QuickplayScreen;
import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.KeySetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ServerData;

/** Helpers for minigame servers. */
public final class ServerModules {

    private ServerModules() {}

    public static boolean onHypixel() {
        Minecraft mc = Minecraft.getMinecraft();
        ServerData data = mc.getCurrentServerData();
        return !mc.isSingleplayer() && data != null && data.serverIP != null
                && data.serverIP.toLowerCase(Locale.ROOT).replaceAll(":[0-9]+$", "").matches("(.*\\.)?hypixel\\.(net|io)\\.?");
    }

    /** Says "gg" in chat when a game ends. */
    public static class AutoGG extends Module {
        // Lines minigames print once a game is over (Hypixel's end-of-game summary).
        private static final String[] END_OF_GAME = {
            "1st Killer - ", "1st Place - ", "Winner: ", " - Damage Dealt - ", "Winning Team -", "1st - ", "Winners: ", "Winner - ",
            "Winners - ", "Top Survivors", "Last team standing!", "Sumo Duel - ", "Top Seller: ",
        };

        private static final java.util.regex.Pattern PLAYER_CHAT =
                java.util.regex.Pattern.compile("^((\\[[^\\]]+\\] )*[A-Za-z0-9_]{1,16}|(Guild|Party|Officer|From|To) .*?): .*");

        private final ModeSetting message = mode("message", "Message", "gg", "gg", "GG", "gg wp", "Good game");
        private final NumberSetting delay = number("delay", "Delay", 1f, 0f, 5f, 0.5f, " s");
        private final BoolSetting hypixelOnly = bool("hypixelonly", "Only on Hypixel", true);

        private int countdown = -1;
        private long lastSent;

        public AutoGG() {
            super("autogg", "Auto GG", "Says gg when a game ends.", Category.CHAT);
        }

        /** A chat line arrived; plain text, no colours. */
        public void onChat(String line) {
            if (!isEnabled() || countdown >= 0 || System.currentTimeMillis() - lastSent < 10000L)
                return;
            Minecraft mc = mc();
            if (mc.thePlayer == null || mc.isSingleplayer() || (hypixelOnly.on() && !onHypixel()))
                return;
            // Someone typing a summary line in chat shouldn't trigger it: game summaries are centred
            // with spaces, player, party and guild chat start with the name or the channel.
            if (PLAYER_CHAT.matcher(line).matches())
                return;
            for (String marker : END_OF_GAME) {
                if (line.contains(marker)) {
                    countdown = Math.round(delay.value() * 20f);
                    return;
                }
            }
        }

        @Override
        public void onTick(Minecraft mc) {
            if (countdown < 0 || countdown-- > 0)
                return;
            countdown = -1;
            if (mc.thePlayer == null)
                return;
            lastSent = System.currentTimeMillis();
            mc.thePlayer.sendChatMessage(onHypixel() ? "/achat " + message.get() : message.get());
        }
    }

    /** A menu of Hypixel games, opened with M, that joins one in a click. */
    public static class Quickplay extends Module {
        private final KeySetting key = key("key", "Menu key", 50);  // M
        private boolean keyWasDown;

        public Quickplay() {
            super("quickplay", "Quickplay", "Press M to pick a Hypixel game and join it.", Category.UTILITY);
        }

        @Override
        public void onTick(Minecraft mc) {
            boolean down = key.isDown();
            if (down && !keyWasDown && mc.currentScreen == null && mc.theWorld != null)
                mc.displayGuiScreen(new QuickplayScreen());
            keyWasDown = down;
        }
    }
}
