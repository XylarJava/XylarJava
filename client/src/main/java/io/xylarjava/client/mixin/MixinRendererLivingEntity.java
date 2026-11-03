package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyArg;
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

    // Hit Color: the red tint of a hurt mob is four floats (red, green, blue, strength) put into a buffer.
    @ModifyArg(method = "setBrightness", at = @At(value = "INVOKE", target = "Ljava/nio/FloatBuffer;put(F)Ljava/nio/FloatBuffer;", remap = false, ordinal = 0))
    private float xylar$hitRed(float value) {
        return Modules.HIT_COLOR.isEnabled() ? Modules.HIT_COLOR.red() : value;
    }

    @ModifyArg(method = "setBrightness", at = @At(value = "INVOKE", target = "Ljava/nio/FloatBuffer;put(F)Ljava/nio/FloatBuffer;", remap = false, ordinal = 1))
    private float xylar$hitGreen(float value) {
        return Modules.HIT_COLOR.isEnabled() ? Modules.HIT_COLOR.green() : value;
    }

    @ModifyArg(method = "setBrightness", at = @At(value = "INVOKE", target = "Ljava/nio/FloatBuffer;put(F)Ljava/nio/FloatBuffer;", remap = false, ordinal = 2))
    private float xylar$hitBlue(float value) {
        return Modules.HIT_COLOR.isEnabled() ? Modules.HIT_COLOR.blue() : value;
    }

    @ModifyArg(method = "setBrightness", at = @At(value = "INVOKE", target = "Ljava/nio/FloatBuffer;put(F)Ljava/nio/FloatBuffer;", remap = false, ordinal = 3))
    private float xylar$hitAlpha(float value) {
        return Modules.HIT_COLOR.isEnabled() ? Modules.HIT_COLOR.alpha() : value;
    }
}
