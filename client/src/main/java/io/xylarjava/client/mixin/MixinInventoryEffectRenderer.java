package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.InventoryEffectRenderer;

@Mixin(InventoryEffectRenderer.class)
public abstract class MixinInventoryEffectRenderer {
    /** Keep the inventory centred and skip the effect list beside it. */
    @Inject(method = "updateActivePotionEffects", at = @At("HEAD"), cancellable = true)
    private void xylar$cleanInventory(CallbackInfo ci) {
        if (Modules.CLEAN_INVENTORY.isEnabled())
            ci.cancel();
    }
}
