package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.entity.AbstractClientPlayer;
import net.minecraft.init.Items;

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
}
