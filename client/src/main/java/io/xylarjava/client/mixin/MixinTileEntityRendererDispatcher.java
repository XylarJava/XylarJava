package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.tileentity.TileEntityRendererDispatcher;
import net.minecraft.tileentity.TileEntity;

@Mixin(TileEntityRendererDispatcher.class)
public abstract class MixinTileEntityRendererDispatcher {
    @Inject(method = "renderTileEntity", at = @At("HEAD"), cancellable = true)
    private void xylar$tileCulling(TileEntity tile, float partialTicks, int destroyStage, CallbackInfo ci) {
        if (Modules.TILE_ENTITY_CULLING.isEnabled() && !Modules.TILE_ENTITY_CULLING.shouldRender(tile))
            ci.cancel();
    }
}
