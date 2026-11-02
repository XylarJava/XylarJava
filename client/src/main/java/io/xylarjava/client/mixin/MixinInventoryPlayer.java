package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.entity.player.InventoryPlayer;

@Mixin(InventoryPlayer.class)
public abstract class MixinInventoryPlayer {
    /** While zooming, the mouse wheel changes the zoom instead of the hotbar slot. */
    @Inject(method = "changeCurrentItem", at = @At("HEAD"), cancellable = true)
    private void xylar$zoomScroll(int direction, CallbackInfo ci) {
        if (Modules.ZOOM.onScroll(direction))
            ci.cancel();
    }
}
