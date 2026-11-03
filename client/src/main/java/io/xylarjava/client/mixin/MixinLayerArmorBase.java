package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.model.ModelBase;
import net.minecraft.client.renderer.GlStateManager;
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

    /** Glint Colorizer on armour (vanilla tints it purple with this call). */
    @Redirect(method = "renderGlint", at = @At(value = "INVOKE", target = "Lnet/minecraft/client/renderer/GlStateManager;color(FFFF)V", ordinal = 1))
    private void xylar$armorGlintColor(float red, float green, float blue, float alpha) {
        if (Modules.GLINT_COLOR.isEnabled()) {
            int c = Modules.GLINT_COLOR.argb();
            // Vanilla's purple is the colour at 76% brightness; keep that so the shine looks the same strength.
            GlStateManager.color(((c >> 16) & 255) / 255f * 0.76f, ((c >> 8) & 255) / 255f * 0.76f, (c & 255) / 255f * 0.76f, alpha);
        } else {
            GlStateManager.color(red, green, blue, alpha);
        }
    }
}
