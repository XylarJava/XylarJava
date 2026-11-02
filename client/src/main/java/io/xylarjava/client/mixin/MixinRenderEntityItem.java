package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.entity.RenderEntityItem;
import net.minecraft.item.ItemStack;

@Mixin(RenderEntityItem.class)
public abstract class MixinRenderEntityItem {
    /** How many copies of a dropped stack to draw. */
    @Inject(method = "func_177078_a", at = @At("HEAD"), cancellable = true)
    private void xylar$compactItems(ItemStack stack, CallbackInfoReturnable<Integer> cir) {
        if (Modules.COMPACT_ITEMS.isEnabled())
            cir.setReturnValue(1);
    }
}
