package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Hooks;
import io.xylarjava.client.Modules;
import net.minecraft.client.gui.GuiPlayerTabOverlay;
import net.minecraft.client.network.NetworkPlayerInfo;

@Mixin(GuiPlayerTabOverlay.class)
public abstract class MixinGuiPlayerTabOverlay {
    @Inject(method = "drawPing", at = @At("HEAD"), cancellable = true)
    private void xylar$numericPing(int width, int x, int y, NetworkPlayerInfo info, CallbackInfo ci) {
        if (Modules.TAB_PING.isEnabled()) {
            Hooks.drawNumericPing(width, x, y, info);
            ci.cancel();
        }
    }
}
