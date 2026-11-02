package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.injection.ModifyArg;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.renderer.ItemRenderer;

@Mixin(ItemRenderer.class)
public abstract class MixinItemRenderer {
    private float xylar$partialTicks;
    private boolean xylar$fireLowered;

    @Inject(method = "renderFireInFirstPerson", at = @At("HEAD"))
    private void xylar$lowFireStart(float partialTicks, CallbackInfo ci) {
        xylar$fireLowered = Modules.LOW_FIRE.isEnabled();
        if (xylar$fireLowered) {
            GlStateManager.pushMatrix();
            GlStateManager.translate(0F, -Modules.LOW_FIRE.offset(), 0F);
        }
    }

    @Inject(method = "renderFireInFirstPerson", at = @At("RETURN"))
    private void xylar$lowFireEnd(float partialTicks, CallbackInfo ci) {
        if (xylar$fireLowered) {
            xylar$fireLowered = false;
            GlStateManager.popMatrix();
        }
    }

    @Inject(method = "renderItemInFirstPerson", at = @At("HEAD"))
    private void xylar$rememberPartialTicks(float partialTicks, CallbackInfo ci) {
        xylar$partialTicks = partialTicks;
    }

    /** 1.7 block-hitting: the third transform call is the BLOCK case; give it the swing progress instead of 0. */
    @ModifyArg(method = "renderItemInFirstPerson",
               at = @At(value = "INVOKE", target = "Lnet/minecraft/client/renderer/ItemRenderer;transformFirstPersonItem(FF)V", ordinal = 2),
               index = 1)
    private float xylar$blockHitSwing(float swingProgress) {
        if (Modules.OLD_ANIMATIONS.isEnabled() && Modules.OLD_ANIMATIONS.blockHit.on() && Minecraft.getMinecraft().thePlayer != null)
            return Minecraft.getMinecraft().thePlayer.getSwingProgress(xylar$partialTicks);
        return swingProgress;
    }
}
