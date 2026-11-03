package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.culling.ICamera;
import net.minecraft.client.renderer.entity.RenderManager;
import net.minecraft.entity.Entity;

@Mixin(RenderManager.class)
public abstract class MixinRenderManager {
    @Inject(method = "shouldRender", at = @At("HEAD"), cancellable = true)
    private void xylar$entityCulling(Entity entity, ICamera camera, double camX, double camY, double camZ, CallbackInfoReturnable<Boolean> cir) {
        if (Modules.ENTITY_CULLING.isEnabled() && !Modules.ENTITY_CULLING.shouldRender(entity, camX, camY, camZ))
            cir.setReturnValue(false);
    }

    /** Name tags turn toward the Freelook camera. */
    @Redirect(method = "cacheActiveRenderInfo", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;rotationYaw:F"))
    private float xylar$viewYaw(Entity entity) {
        return Modules.FREELOOK.yaw(entity, entity.rotationYaw);
    }

    @Redirect(method = "cacheActiveRenderInfo", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;prevRotationYaw:F"))
    private float xylar$viewPrevYaw(Entity entity) {
        return Modules.FREELOOK.yaw(entity, entity.prevRotationYaw);
    }

    @Redirect(method = "cacheActiveRenderInfo", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;rotationPitch:F"))
    private float xylar$viewPitch(Entity entity) {
        return Modules.FREELOOK.pitch(entity, entity.rotationPitch);
    }

    @Redirect(method = "cacheActiveRenderInfo", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/Entity;prevRotationPitch:F"))
    private float xylar$viewPrevPitch(Entity entity) {
        return Modules.FREELOOK.pitch(entity, entity.prevRotationPitch);
    }
}
