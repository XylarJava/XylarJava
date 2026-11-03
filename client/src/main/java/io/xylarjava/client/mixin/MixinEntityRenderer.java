package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Hooks;
import io.xylarjava.client.Modules;
import io.xylarjava.client.XylarClient;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.client.renderer.EntityRenderer;
import net.minecraft.entity.Entity;

@Mixin(EntityRenderer.class)
public abstract class MixinEntityRenderer {
    @Shadow
    private float fogColorRed;
    @Shadow
    private float fogColorGreen;
    @Shadow
    private float fogColorBlue;

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

    @Inject(method = "updateFogColor", at = @At("RETURN"))
    private void xylar$fogColor(float partialTicks, CallbackInfo ci) {
        float[] rgb = Hooks.fogColor(fogColorRed, fogColorGreen, fogColorBlue, partialTicks);
        if (rgb != null) {
            fogColorRed = rgb[0];
            fogColorGreen = rgb[1];
            fogColorBlue = rgb[2];
        }
    }

    /** Menu Blur and Motion Blur pick their shader before the frame is drawn. */
    @Inject(method = "updateCameraAndRender", at = @At("HEAD"))
    private void xylar$shaders(float partialTicks, long nanoTime, CallbackInfo ci) {
        Hooks.beforeFrame((EntityRenderer) (Object) this);
    }

    /** While Freelook is held the mouse turns the camera, not the player. */
    @Redirect(method = "updateCameraAndRender",
              at = @At(value = "INVOKE", target = "Lnet/minecraft/client/entity/EntityPlayerSP;setAngles(FF)V"))
    private void xylar$freelookTurn(EntityPlayerSP player, float yaw, float pitch) {
        if (!Modules.FREELOOK.turn(yaw, pitch))
            player.setAngles(yaw, pitch);
    }

    @Redirect(method = "orientCamera", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;rotationYaw:F"))
    private float xylar$cameraYaw(Entity entity) {
        return Modules.FREELOOK.yaw(entity, entity.rotationYaw);
    }

    @Redirect(method = "orientCamera", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;prevRotationYaw:F"))
    private float xylar$cameraPrevYaw(Entity entity) {
        return Modules.FREELOOK.yaw(entity, entity.prevRotationYaw);
    }

    @Redirect(method = "orientCamera", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;rotationPitch:F"))
    private float xylar$cameraPitch(Entity entity) {
        return Modules.FREELOOK.pitch(entity, entity.rotationPitch);
    }

    @Redirect(method = "orientCamera", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;prevRotationPitch:F"))
    private float xylar$cameraPrevPitch(Entity entity) {
        return Modules.FREELOOK.pitch(entity, entity.prevRotationPitch);
    }

    /** Waypoints are drawn with the world, just before the hand. */
    @Inject(method = "renderWorldPass", at = @At(value = "INVOKE_STRING", target = "Lnet/minecraft/profiler/Profiler;endStartSection(Ljava/lang/String;)V",
                                                 args = "ldc=hand"))
    private void xylar$worldOverlays(int pass, float partialTicks, long finishTimeNano, CallbackInfo ci) {
        Hooks.renderWorldOverlays(partialTicks);
    }
}
