package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Hooks;
import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.entity.RenderEntityItem;
import net.minecraft.client.resources.model.IBakedModel;
import net.minecraft.entity.item.EntityItem;
import net.minecraft.item.ItemStack;

@Mixin(RenderEntityItem.class)
public abstract class MixinRenderEntityItem {
    @Shadow
    private int func_177078_a(ItemStack stack) {
        return 1;
    }

    /** Item Physics: dropped items lie flat on the ground instead of floating and spinning. */
    @Inject(method = "func_177077_a", at = @At("HEAD"), cancellable = true)
    private void xylar$itemPhysics(EntityItem item, double x, double y, double z, float partialTicks, IBakedModel model,
                                   CallbackInfoReturnable<Integer> cir) {
        if (!Modules.ITEM_PHYSICS.isEnabled())
            return;
        ItemStack stack = item.getEntityItem();
        if (stack == null || stack.getItem() == null) {
            cir.setReturnValue(0);
            return;
        }
        cir.setReturnValue(Hooks.placeDroppedItem(item, x, y, z, partialTicks, model, func_177078_a(stack)));
    }

    /** How many copies of a dropped stack to draw. */
    @Inject(method = "func_177078_a", at = @At("HEAD"), cancellable = true)
    private void xylar$compactItems(ItemStack stack, CallbackInfoReturnable<Integer> cir) {
        if (Modules.COMPACT_ITEMS.isEnabled())
            cir.setReturnValue(1);
    }
}
