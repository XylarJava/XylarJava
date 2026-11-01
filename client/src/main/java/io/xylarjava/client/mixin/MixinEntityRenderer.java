package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.XylarClient;
import net.minecraft.client.renderer.EntityRenderer;

@Mixin(EntityRenderer.class)
public abstract class MixinEntityRenderer {

    @Inject(method = "getFOVModifier", at = @At("RETURN"), cancellable = true)
    private void xylar$zoom(float partialTicks, boolean useFOVSetting, CallbackInfoReturnable<Float> cir) {
        cir.setReturnValue(XylarClient.get().modifyFov(cir.getReturnValue()));
    }
}
