package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import io.xylarjava.client.XylarClient;
import net.minecraft.client.gui.GuiIngame;

@Mixin(GuiIngame.class)
public abstract class MixinGuiIngame {

    @Inject(method = "renderGameOverlay", at = @At("RETURN"))
    private void xylar$renderHud(float partialTicks, CallbackInfo ci) {
        XylarClient.get().renderHud();
    }
}
