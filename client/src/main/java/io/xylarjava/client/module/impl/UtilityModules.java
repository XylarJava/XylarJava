package io.xylarjava.client.module.impl;

import java.awt.Image;
import java.awt.Toolkit;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.io.File;

import javax.imageio.ImageIO;

import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.event.ClickEvent;
import net.minecraft.event.HoverEvent;
import net.minecraft.item.ItemStack;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.ChatComponentTranslation;
import net.minecraft.util.ChatStyle;
import net.minecraft.util.EnumChatFormatting;
import net.minecraft.util.IChatComponent;

/** Everything else that makes the game nicer to use. */
public final class UtilityModules {

    private UtilityModules() {}

    /**
     * F11 gives a borderless window that covers the screen instead of exclusive fullscreen,
     * so alt-tabbing is instant.
     */
    public static class BorderlessFullscreen extends Module {
        private boolean active;
        private int savedWidth = 854;
        private int savedHeight = 480;

        public BorderlessFullscreen() {
            super("borderless", "Borderless Fullscreen", "Instant alt-tab: F11 makes a borderless window.", Category.UTILITY);
        }

        /** Returns true when F11 was handled here. */
        public boolean toggle(Minecraft mc) {
            try {
                if (!active) {
                    savedWidth = Math.max(1, mc.displayWidth);
                    savedHeight = Math.max(1, mc.displayHeight);
                    DisplayMode desktop = Display.getDesktopDisplayMode();
                    System.setProperty("org.lwjgl.opengl.Window.undecorated", "true");
                    Display.setFullscreen(false);
                    Display.setDisplayMode(new DisplayMode(desktop.getWidth(), desktop.getHeight()));
                    Display.setLocation(0, 0);
                    Display.setResizable(false);
                    active = true;
                } else {
                    System.setProperty("org.lwjgl.opengl.Window.undecorated", "false");
                    Display.setDisplayMode(new DisplayMode(savedWidth, savedHeight));
                    Display.setResizable(true);
                    Display.setLocation(-1, -1);
                    active = false;
                }
                ((io.xylarjava.client.mixin.MinecraftAccessor) mc).xylar$resize(Display.getWidth(), Display.getHeight());
                return true;
            } catch (Exception e) {
                System.err.println("[XylarJava] Borderless fullscreen failed, using normal fullscreen: " + e);
                return false;
            }
        }
    }

    public static class WindowTitle extends Module {
        public WindowTitle() {
            super("windowtitle", "Window Title", "Name the window XylarJava Client 1.8.9.", Category.UTILITY);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        @Override
        public void onTick(Minecraft mc) {
            if (!"XylarJava Client 1.8.9".equals(Display.getTitle()))
                Display.setTitle("XylarJava Client 1.8.9");
        }

        @Override
        protected void onDisable() {
            Display.setTitle("Minecraft 1.8.9");
        }
    }

    /** A chat warning when a piece of armour is about to break. */
    public static class ArmorWarning extends Module {
        private final NumberSetting percent = number("percent", "Warn below", 10f, 5f, 50f, 5f, "%");
        private final boolean[] warned = new boolean[4];

        public ArmorWarning() {
            super("armorwarning", "Low Armour Warning", "Tells you when armour is about to break.", Category.UTILITY);
        }

        @Override
        public void onTick(Minecraft mc) {
            EntityPlayerSP p = mc.thePlayer;
            if (p == null)
                return;
            String[] names = {"Boots", "Leggings", "Chestplate", "Helmet"};
            for (int i = 0; i < 4; i++) {
                ItemStack stack = p.inventory.armorInventory[i];
                if (stack == null || !stack.isItemStackDamageable()) {
                    warned[i] = false;
                    continue;
                }
                int left = stack.getMaxDamage() - stack.getItemDamage();
                boolean low = left * 100 < stack.getMaxDamage() * percent.intValue();
                if (low && !warned[i])
                    p.addChatMessage(new ChatComponentText("\u00a76[XylarJava] \u00a7eYour " + names[i] + " is about to break (" + left + " uses left)."));
                warned[i] = low;
            }
        }
    }

    public static class CustomMainMenu extends Module {
        public CustomMainMenu() {
            super("custommainmenu", "XylarJava Title Screen", "Use the XylarJava title screen.", Category.UTILITY);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }
    }

    public static class PauseOnLostFocus extends Module {
        private final BoolSetting pause = bool("pause", "Pause when tabbed out", false);

        public PauseOnLostFocus() {
            super("pausefocus", "Focus Pause", "Choose whether alt-tab opens the pause menu.", Category.UTILITY);
        }

        @Override
        public void onTick(Minecraft mc) {
            if (mc.gameSettings.pauseOnLostFocus != pause.on())
                mc.gameSettings.pauseOnLostFocus = pause.on();
        }
    }

    /** After F2: copies the screenshot so it can be pasted straight away, and adds Open and Folder links. */
    public static class ScreenshotHelper extends Module {
        private final BoolSetting copy = bool("copy", "Copy to clipboard", true);
        private final BoolSetting links = bool("links", "Open and Folder links", true);

        public ScreenshotHelper() {
            super("screenshots", "Screenshot Helper", "Copy screenshots to the clipboard and open them from chat.", Category.UTILITY);
        }

        @Override
        protected boolean enabledByDefault() {
            return true;
        }

        public IChatComponent saved(File gameDir, IChatComponent message) {
            File file = savedFile(message);
            if (file == null || !file.isFile())
                return message;  // saving failed: keep the game's error message
            if (copy.on())
                copyToClipboard(file);
            if (links.on()) {
                message.appendSibling(link(" [Open]", file, "Open the screenshot"));
                message.appendSibling(link(" [Folder]", file.getParentFile(), "Open the screenshots folder"));
            }
            if (copy.on())
                message.appendSibling(new ChatComponentText(" Copied").setChatStyle(new ChatStyle().setColor(EnumChatFormatting.GRAY)));
            return message;
        }

        private static IChatComponent link(String text, File file, String hover) {
            ChatStyle style = new ChatStyle().setColor(EnumChatFormatting.GOLD)
                    .setChatClickEvent(new ClickEvent(ClickEvent.Action.OPEN_FILE, file.getAbsolutePath()))
                    .setChatHoverEvent(new HoverEvent(HoverEvent.Action.SHOW_TEXT, new ChatComponentText(hover)));
            return new ChatComponentText(text).setChatStyle(style);
        }

        /** The game links the file name to the file; find that link. */
        private static File savedFile(IChatComponent component) {
            ClickEvent click = component.getChatStyle().getChatClickEvent();
            if (click != null && click.getAction() == ClickEvent.Action.OPEN_FILE)
                return new File(click.getValue());
            if (component instanceof ChatComponentTranslation) {
                for (Object arg : ((ChatComponentTranslation) component).getFormatArgs()) {
                    if (arg instanceof IChatComponent) {
                        File file = savedFile((IChatComponent) arg);
                        if (file != null)
                            return file;
                    }
                }
            }
            for (IChatComponent sibling : component.getSiblings()) {
                File file = savedFile(sibling);
                if (file != null)
                    return file;
            }
            return null;
        }

        private static void copyToClipboard(final File file) {
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    try {
                        final Image image = ImageIO.read(file);
                        if (image == null)
                            return;
                        Toolkit.getDefaultToolkit().getSystemClipboard().setContents(new Transferable() {
                            @Override
                            public DataFlavor[] getTransferDataFlavors() {
                                return new DataFlavor[] {DataFlavor.imageFlavor};
                            }

                            @Override
                            public boolean isDataFlavorSupported(DataFlavor flavor) {
                                return DataFlavor.imageFlavor.equals(flavor);
                            }

                            @Override
                            public Object getTransferData(DataFlavor flavor) throws UnsupportedFlavorException {
                                if (!isDataFlavorSupported(flavor))
                                    throw new UnsupportedFlavorException(flavor);
                                return image;
                            }
                        }, null);
                    } catch (Throwable e) {
                        System.err.println("[XylarJava] Couldn't copy the screenshot: " + e);
                    }
                }
            }, "XylarJava screenshot copy");
            thread.setDaemon(true);
            thread.start();
        }
    }
}
