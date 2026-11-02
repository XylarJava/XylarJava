package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.Module;
import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;

/** Keeps you sprinting while you move forward, like holding the sprint key. */
public class ToggleSprintModule extends Module {

    public ToggleSprintModule() {
        super("togglesprint", "Toggle Sprint", "Sprint without holding the key.");
    }

    @Override
    public void onTick(Minecraft mc) {
        EntityPlayerSP player = mc.thePlayer;
        if (player == null || mc.currentScreen != null)
            return;
        boolean canSprint = player.movementInput != null && player.movementInput.moveForward > 0f && !player.isSneaking()
                && !player.isCollidedHorizontally && (player.getFoodStats().getFoodLevel() > 6 || player.capabilities.allowFlying)
                && !player.isUsingItem();
        if (canSprint && !player.isSprinting())
            player.setSprinting(true);
    }
}
