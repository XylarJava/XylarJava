package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Hooks;
import io.xylarjava.client.Modules;
import io.xylarjava.client.XylarClient;
import net.minecraft.client.renderer.EntityRenderer;

@Mixin(EntityRenderer.class)
public abstract class MixinEntityRenderer {
    @Inject(method = "getFOVModifier", at = @At("RETURN"), cancellable = true)
    private void xylar$zoom(float partialTicks, boolean useFOVSetting, CallbackInfoReturnable<Float> cir) {
        cir.setReturnValue(XylarClient.get().modifyFov(cir.getReturnValue()));
    }

    @Inject(method = "renderRainSnow", at = @At("HEAD"), cancellable = true)
    private void xylar$noRain(float partialTicks, CallbackInfo ci) {
        if (Modules.WEATHER_EFFECTS.isEnabled() && Modules.WEATHER_EFFECTS.rain.on())
            ci.cancel();
    }

    @Inject(method = "addRainParticles", at = @At("HEAD"), cancellable = true)
    private void xylar$noRainSplashes(CallbackInfo ci) {
        if (Modules.WEATHER_EFFECTS.isEnabled() && Modules.WEATHER_EFFECTS.splashes.on())
            ci.cancel();
    }

    @Inject(method = "setupFog", at = @At("RETURN"))
    private void xylar$noFog(int startCoords, float partialTicks, CallbackInfo ci) {
        Hooks.afterSetupFog(startCoords, partialTicks);
    }

    @Inject(method = "hurtCameraEffect", at = @At("HEAD"), cancellable = true)
    private void xylar$noHurtCam(float partialTicks, CallbackInfo ci) {
        if (Modules.NO_HURT_CAM.isEnabled())
            ci.cancel();
    }
}
