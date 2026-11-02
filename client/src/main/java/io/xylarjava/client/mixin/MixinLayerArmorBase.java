package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.model.ModelBase;
import net.minecraft.client.renderer.entity.layers.LayerArmorBase;
import net.minecraft.entity.EntityLivingBase;

@Mixin(LayerArmorBase.class)
public abstract class MixinLayerArmorBase {
    @Inject(method = "renderGlint", at = @At("HEAD"), cancellable = true)
    private void xylar$noArmorGlint(EntityLivingBase entity, ModelBase model, float limbSwing, float limbSwingAmount, float partialTicks,
                                    float ageInTicks, float netHeadYaw, float headPitch, float scale, CallbackInfo ci) {
        if (Modules.NO_GLINT.isEnabled() && Modules.NO_GLINT.armor.on())
            ci.cancel();
    }
}
