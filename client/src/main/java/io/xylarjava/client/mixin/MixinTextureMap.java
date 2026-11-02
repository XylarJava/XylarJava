package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.texture.TextureMap;

@Mixin(TextureMap.class)
public abstract class MixinTextureMap {
    @Inject(method = "updateAnimations", at = @At("HEAD"), cancellable = true)
    private void xylar$staticTextures(CallbackInfo ci) {
        if (Modules.STATIC_TEXTURES.isEnabled())
            ci.cancel();
    }
}
