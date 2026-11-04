package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.entity.AbstractClientPlayer;
import net.minecraft.init.Items;
import net.minecraft.util.ResourceLocation;

@Mixin(AbstractClientPlayer.class)
public abstract class MixinAbstractClientPlayer {
    @Inject(method = "getFovModifier", at = @At("RETURN"), cancellable = true)
    private void xylar$staticFov(CallbackInfoReturnable<Float> cir) {
        if (!Modules.STATIC_FOV.isEnabled())
            return;
        AbstractClientPlayer self = (AbstractClientPlayer) (Object) this;
        if (Modules.STATIC_FOV.keepBow() && self.isUsingItem() && self.getItemInUse() != null && self.getItemInUse().getItem() == Items.bow)
            return;
        cir.setReturnValue(1.0F);
    }

    /** A XylarJava cape, when the player wears one, over the vanilla cape. */
    @Inject(method = "getLocationCape", at = @At("HEAD"), cancellable = true)
    private void xylar$xylarCape(CallbackInfoReturnable<ResourceLocation> cir) {
        AbstractClientPlayer self = (AbstractClientPlayer) (Object) this;
        ResourceLocation cape = Modules.CAPES.capeFor(self.getGameProfile().getName());
        if (cape != null)
            cir.setReturnValue(cape);
    }
}
