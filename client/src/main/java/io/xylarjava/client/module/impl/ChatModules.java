package io.xylarjava.client.module.impl;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.audio.PositionedSoundRecord;
import net.minecraft.util.ResourceLocation;

/** Chat helpers. The message handling itself lives in {@link io.xylarjava.client.Hooks#onChatMessage}. */
public final class ChatModules {

    private ChatModules() {}

    public static class Timestamps extends Module {
        private final ModeSetting format = mode("format", "Format", "24 hour", "24 hour", "12 hour");
        private final BoolSetting seconds = bool("seconds", "Show seconds", false);

        public Timestamps() {
            super("timestamps", "Chat Timestamps", "The time next to every message.", Category.CHAT);
        }

        public String stamp() {
            String pattern = format.is("24 hour") ? (seconds.on() ? "HH:mm:ss" : "HH:mm") : (seconds.on() ? "h:mm:ss a" : "h:mm a");
            return "§7[" + new SimpleDateFormat(pattern, Locale.ENGLISH).format(new Date()) + "] §r";
        }
    }

    public static class CompactChat extends Module {
        public CompactChat() {
            super("compactchat", "Compact Chat", "Repeated messages stack as (x2), (x3)...", Category.CHAT);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }
    }

    public static class ChatHistory extends Module {
        private final NumberSetting lines = number("lines", "Lines kept", 500f, 100f, 2000f, 100f, "");

        public ChatHistory() {
            super("chathistory", "Longer Chat History", "Keep more than 100 chat lines.", Category.CHAT);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        public int lines() {
            return lines.intValue();
        }
    }

    public static class NameHider extends Module {
        private final ModeSetting replacement = mode("replacement", "Show as", "You", "You", "Player", "Hidden", "XylarJava User");

        public NameHider() {
            super("namehider", "Name Hider", "Hide your name in chat, tab and nametags (for videos).", Category.CHAT);
        }

        public String replacement() {
            return replacement.is("Hidden") ? "§k" + "Hidden" + "§r" : replacement.get();
        }
    }

    public static class MentionPing extends Module {
        public MentionPing() {
            super("mentionping", "Mention Sound", "A sound when someone writes your name.", Category.CHAT);
        }

        public void ping() {
            mc().getSoundHandler().playSound(PositionedSoundRecord.create(new ResourceLocation("random.orb"), 1f));
        }
    }
}
