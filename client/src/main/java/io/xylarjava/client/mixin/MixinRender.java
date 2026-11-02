package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.entity.Render;
import net.minecraft.entity.Entity;

@Mixin(Render.class)
public abstract class MixinRender {
    @Inject(method = "renderEntityOnFire", at = @At("HEAD"), cancellable = true)
    private void xylar$hideEntityFire(Entity entity, double x, double y, double z, float partialTicks, CallbackInfo ci) {
        if (Modules.HIDE_ENTITY_FIRE.isEnabled())
            ci.cancel();
    }
}
