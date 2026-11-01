package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

import net.minecraft.client.gui.GuiMainMenu;

/** Lets our title screen reuse the vanilla rotating panorama. */
@Mixin(GuiMainMenu.class)
public interface GuiMainMenuAccessor {

    @Invoker("renderSkybox")
    void xylar$renderSkybox(int mouseX, int mouseY, float partialTicks);
}
