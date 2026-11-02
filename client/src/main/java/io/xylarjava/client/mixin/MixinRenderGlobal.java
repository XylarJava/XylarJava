package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Hooks;
import io.xylarjava.client.Modules;
import net.minecraft.client.multiplayer.WorldClient;
import net.minecraft.client.renderer.RenderGlobal;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.util.MovingObjectPosition;

@Mixin(RenderGlobal.class)
public abstract class MixinRenderGlobal {
    @Shadow
    private WorldClient theWorld;

    @Inject(method = "renderSky(FI)V", at = @At("HEAD"), cancellable = true)
    private void xylar$noSky(float partialTicks, int pass, CallbackInfo ci) {
        if (Modules.NO_SKY.isEnabled())
            ci.cancel();
    }

    @Inject(method = "drawSelectionBox", at = @At("HEAD"), cancellable = true)
    private void xylar$blockOutline(EntityPlayer player, MovingObjectPosition hit, int execute, float partialTicks, CallbackInfo ci) {
        if (Hooks.drawBlockOutline(this.theWorld, player, hit, execute, partialTicks))
            ci.cancel();
    }
}
