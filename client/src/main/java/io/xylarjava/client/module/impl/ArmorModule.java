package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.renderer.RenderHelper;
import net.minecraft.item.ItemStack;

/** Worn armour and the held item, with durability. */
public class ArmorModule extends HudModule {

    public ArmorModule() {
        super("armor", "Armor", "Armour and held item durability.", 0.01f, 0.4f);
    }

    private final io.xylarjava.client.setting.BoolSetting held = bool("held", "Show held item", true);
    private final io.xylarjava.client.setting.ModeSetting durability = mode("durability", "Durability as", "Uses left", "Uses left", "Percent");

    @Override
    protected boolean enabledByDefault() {
        return true;
    }

    @Override
    public int width() {
        return 58;
    }

    @Override
    public int height() {
        return 5 * 17 + 2;
    }

    @Override
    protected void draw(boolean editing) {
        EntityPlayerSP player = mc().thePlayer;
        ItemStack[] stacks = new ItemStack[5];
        boolean any = false;
        if (player != null) {
            for (int i = 0; i < 4; i++)
                stacks[i] = player.inventory.armorInventory[3 - i];
            stacks[4] = held.on() ? player.getHeldItem() : null;
            for (ItemStack stack : stacks)
                any |= stack != null;
        }
        // Nothing worn or held: no empty box on screen (the HUD editor still shows where it is).
        if (!any && !editing)
            return;
        drawBackground(width(), height());
        if (player == null)
            return;

        RenderHelper.enableGUIStandardItemLighting();
        for (int i = 0; i < stacks.length; i++) {
            ItemStack stack = stacks[i];
            if (stack == null)
                continue;
            int y = 1 + i * 17;
            mc().getRenderItem().renderItemAndEffectIntoGUI(stack, 2, y);
            mc().getRenderItem().renderItemOverlays(mc().fontRendererObj, stack, 2, y);
            if (stack.isItemStackDamageable()) {
                int left = stack.getMaxDamage() - stack.getItemDamage();
                String text = durability.is("Percent") ? (left * 100 / Math.max(1, stack.getMaxDamage())) + "%" : String.valueOf(left);
                drawText(text, 21, y + 4, textColor());
            } else if (stack.stackSize > 1) {
                drawText("x" + stack.stackSize, 21, y + 4, textColor());
            }
        }
        RenderHelper.disableStandardItemLighting();
        GlStateManager.disableLighting();
        GlStateManager.enableBlend();
    }
}
