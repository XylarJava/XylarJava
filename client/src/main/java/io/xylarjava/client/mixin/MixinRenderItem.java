package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyArg;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.entity.RenderItem;
import net.minecraft.client.resources.model.IBakedModel;

@Mixin(RenderItem.class)
public abstract class MixinRenderItem {
    @Inject(method = "renderEffect", at = @At("HEAD"), cancellable = true)
    private void xylar$noGlint(IBakedModel model, CallbackInfo ci) {
        if (Modules.NO_GLINT.isEnabled() && Modules.NO_GLINT.items.on())
            ci.cancel();
    }

    /** Glint Colorizer: the enchantment shine's colour on items. */
    @ModifyArg(method = "renderEffect", at = @At(value = "INVOKE",
               target = "Lnet/minecraft/client/renderer/entity/RenderItem;renderModel(Lnet/minecraft/client/resources/model/IBakedModel;I)V"), index = 1)
    private int xylar$glintColor(int color) {
        return Modules.GLINT_COLOR.isEnabled() ? Modules.GLINT_COLOR.argb() : color;
    }
}
