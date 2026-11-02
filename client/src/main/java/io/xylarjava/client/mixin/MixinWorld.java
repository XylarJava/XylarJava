package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.world.World;

@Mixin(World.class)
public abstract class MixinWorld {
    /** Clear Weather only affects the client world, never the singleplayer server. */
    @Inject(method = "getRainStrength", at = @At("HEAD"), cancellable = true)
    private void xylar$noRain(float delta, CallbackInfoReturnable<Float> cir) {
        if (Modules.CLEAR_WEATHER.isEnabled() && ((World) (Object) this).isRemote)
            cir.setReturnValue(0.0F);
    }

    @Inject(method = "getThunderStrength", at = @At("HEAD"), cancellable = true)
    private void xylar$noThunder(float delta, CallbackInfoReturnable<Float> cir) {
        if (Modules.CLEAR_WEATHER.isEnabled() && ((World) (Object) this).isRemote)
            cir.setReturnValue(0.0F);
    }
}
