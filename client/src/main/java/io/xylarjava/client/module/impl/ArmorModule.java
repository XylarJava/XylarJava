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
        Draw.rect(0, 0, width(), height(), Draw.PANEL);
        EntityPlayerSP player = mc().thePlayer;
        if (player == null)
            return;

        ItemStack[] stacks = new ItemStack[5];
        for (int i = 0; i < 4; i++)
            stacks[i] = player.inventory.armorInventory[3 - i];
        stacks[4] = player.getHeldItem();

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
                mc().fontRendererObj.drawStringWithShadow(String.valueOf(left), 21, y + 4, Draw.TEXT);
            } else if (stack.stackSize > 1) {
                mc().fontRendererObj.drawStringWithShadow("x" + stack.stackSize, 21, y + 4, Draw.TEXT);
            }
        }
        RenderHelper.disableStandardItemLighting();
        GlStateManager.disableLighting();
        GlStateManager.enableBlend();
    }
}
