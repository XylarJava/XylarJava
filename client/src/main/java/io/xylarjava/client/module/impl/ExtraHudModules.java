package io.xylarjava.client.module.impl;

import java.util.ArrayList;
import java.util.List;

import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.module.TextHudModule;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.KeySetting;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.client.gui.Gui;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.renderer.RenderHelper;
import net.minecraft.client.resources.ResourcePackRepository;
import net.minecraft.init.Blocks;
import net.minecraft.init.Items;
import net.minecraft.item.Item;
import net.minecraft.item.ItemBlock;
import net.minecraft.item.ItemPotion;
import net.minecraft.item.ItemStack;
import net.minecraft.util.EnumChatFormatting;

/** More HUD elements: items you carry, your resource pack and a stopwatch. */
public final class ExtraHudModules {

    private ExtraHudModules() {}

    /** How many of the things that matter in a fight you have left (whole inventory, not just the hotbar). */
    public static class ItemCounter extends HudModule {
        private final BoolSetting gapples = bool("gapples", "Golden apples", true);
        private final BoolSetting pearls = bool("pearls", "Ender pearls", true);
        private final BoolSetting arrows = bool("arrows", "Arrows", false);
        private final BoolSetting potions = bool("potions", "Splash potions", true);
        private final BoolSetting blocks = bool("blocks", "Blocks", true);
        private final BoolSetting hideEmpty = bool("hideempty", "Hide what you don't have", true);

        private final List<ItemStack> icons = new ArrayList<ItemStack>();
        private final List<Integer> counts = new ArrayList<Integer>();

        public ItemCounter() {
            super("itemcounter", "Item Counter", "Golden apples, pearls, potions and blocks you have left.", 0.01f, 0.62f);
        }

        private void collect(boolean editing) {
            icons.clear();
            counts.clear();
            EntityPlayerSP player = mc().thePlayer;
            int gapple = 0, pearl = 0, arrow = 0, potion = 0, block = 0;
            if (player != null) {
                for (ItemStack stack : player.inventory.mainInventory) {
                    if (stack == null || stack.getItem() == null)
                        continue;
                    Item item = stack.getItem();
                    if (item == Items.golden_apple)
                        gapple += stack.stackSize;
                    else if (item == Items.ender_pearl)
                        pearl += stack.stackSize;
                    else if (item == Items.arrow)
                        arrow += stack.stackSize;
                    else if (item instanceof ItemPotion && ItemPotion.isSplash(stack.getMetadata()))
                        potion += stack.stackSize;
                    else if (item instanceof ItemBlock && ((ItemBlock) item).getBlock().isFullCube())
                        block += stack.stackSize;
                }
            }
            add(gapples.on(), new ItemStack(Items.golden_apple), gapple, editing);
            add(pearls.on(), new ItemStack(Items.ender_pearl), pearl, editing);
            add(arrows.on(), new ItemStack(Items.arrow), arrow, editing);
            add(potions.on(), new ItemStack(Items.potionitem, 1, 16421), potion, editing);
            add(blocks.on(), new ItemStack(Blocks.wool), block, editing);
        }

        private void add(boolean shown, ItemStack icon, int count, boolean editing) {
            if (!shown || (count == 0 && hideEmpty.on() && !editing))
                return;
            icons.add(icon);
            counts.add(count);
        }

        @Override
        public int width() {
            return 44;
        }

        @Override
        public int height() {
            collect(true);
            return Math.max(1, icons.size()) * 17 + 2;
        }

        @Override
        protected void draw(boolean editing) {
            collect(editing);
            if (icons.isEmpty())
                return;
            drawBackground(width(), icons.size() * 17 + 2);
            RenderHelper.enableGUIStandardItemLighting();
            for (int i = 0; i < icons.size(); i++)
                mc().getRenderItem().renderItemAndEffectIntoGUI(icons.get(i), 2, 1 + i * 17);
            RenderHelper.disableStandardItemLighting();
            GlStateManager.disableLighting();
            GlStateManager.enableBlend();
            for (int i = 0; i < icons.size(); i++) {
                int count = counts.get(i);
                drawText(String.valueOf(count), 21, 5 + i * 17, count == 0 ? 0xFFFF5555 : textColor());
            }
        }
    }

    /** The resource pack on top, with its icon. */
    public static class PackDisplay extends HudModule {
        public PackDisplay() {
            super("packdisplay", "Pack Display", "The resource pack you're using.", 0.01f, 0.9f);
        }

        private ResourcePackRepository.Entry top() {
            List<ResourcePackRepository.Entry> entries = mc().getResourcePackRepository().getRepositoryEntries();
            return entries.isEmpty() ? null : entries.get(entries.size() - 1);
        }

        private String packName() {
            ResourcePackRepository.Entry entry = top();
            if (entry == null)
                return "Default";
            String name = EnumChatFormatting.getTextWithoutFormattingCodes(entry.getResourcePackName());
            if (name.toLowerCase().endsWith(".zip"))
                name = name.substring(0, name.length() - 4);
            return name.length() > 32 ? name.substring(0, 31) + "..." : name;
        }

        @Override
        public int width() {
            return 26 + mc().fontRendererObj.getStringWidth(packName());
        }

        @Override
        public int height() {
            return 20;
        }

        @Override
        protected void draw(boolean editing) {
            drawBackground(width(), height());
            ResourcePackRepository.Entry entry = top();
            GlStateManager.color(1f, 1f, 1f, 1f);
            if (entry != null) {
                entry.bindTexturePackIcon(mc().getTextureManager());
                Gui.drawModalRectWithCustomSizedTexture(2, 2, 0, 0, 16, 16, 16, 16);
            } else {
                Draw.rect(2, 2, 16, 16, 0x55FFFFFF);
            }
            drawText(packName(), 22, 6, textColor());
        }
    }

    /** Start and stop with K, reset with L. */
    public static class Stopwatch extends TextHudModule {
        private final KeySetting startStop = key("startstop", "Start / stop key", 37);  // K
        private final KeySetting reset = key("reset", "Reset key", 38);              // L

        private long startedAt;
        private long elapsed;
        private boolean running;
        private boolean startWasDown;
        private boolean resetWasDown;

        public Stopwatch() {
            super("stopwatch", "Stopwatch", "Press K to start or stop, L to reset.", 0.45f, 0.08f);
        }

        @Override
        public void onTick(Minecraft mc) {
            boolean start = mc.currentScreen == null && startStop.isDown();
            boolean clear = mc.currentScreen == null && reset.isDown();
            if (start && !startWasDown) {
                if (running) {
                    elapsed += System.currentTimeMillis() - startedAt;
                    running = false;
                } else {
                    startedAt = System.currentTimeMillis();
                    running = true;
                }
            }
            if (clear && !resetWasDown) {
                elapsed = 0;
                startedAt = System.currentTimeMillis();
            }
            startWasDown = start;
            resetWasDown = clear;
        }

        @Override
        protected String text() {
            long ms = elapsed + (running ? System.currentTimeMillis() - startedAt : 0);
            long minutes = ms / 60000L;
            long seconds = (ms / 1000L) % 60L;
            long hundredths = (ms / 10L) % 100L;
            return String.format("%s%02d:%02d.%02d", running ? "" : "§7", minutes, seconds, hundredths);
        }
    }
}
