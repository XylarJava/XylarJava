package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
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
}
