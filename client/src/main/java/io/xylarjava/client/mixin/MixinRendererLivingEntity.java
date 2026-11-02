package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.entity.RendererLivingEntity;
import net.minecraft.entity.EntityLivingBase;

@Mixin(RendererLivingEntity.class)
public abstract class MixinRendererLivingEntity {
    @Inject(method = "canRenderName", at = @At("HEAD"), cancellable = true)
    private void xylar$ownNametag(EntityLivingBase entity, CallbackInfoReturnable<Boolean> cir) {
        Minecraft mc = Minecraft.getMinecraft();
        if (Modules.OWN_NAMETAG.isEnabled() && entity == mc.thePlayer && mc.gameSettings.thirdPersonView != 0 && !mc.gameSettings.hideGUI
                && !entity.isInvisible())
            cir.setReturnValue(true);
    }
}
