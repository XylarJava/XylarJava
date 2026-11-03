package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.entity.Render;
import net.minecraft.client.renderer.entity.RenderManager;
import net.minecraft.client.renderer.entity.RenderTNTPrimed;
import net.minecraft.entity.item.EntityTNTPrimed;

@Mixin(RenderTNTPrimed.class)
public abstract class MixinRenderTNTPrimed extends Render<EntityTNTPrimed> {

    protected MixinRenderTNTPrimed(RenderManager manager) {
        super(manager);
    }

    /** TNT Countdown: seconds left above lit TNT. */
    @Inject(method = "doRender(Lnet/minecraft/entity/item/EntityTNTPrimed;DDDFF)V", at = @At("RETURN"))
    private void xylar$tntCountdown(EntityTNTPrimed tnt, double x, double y, double z, float yaw, float partialTicks, CallbackInfo ci) {
        if (Modules.TNT_COUNTDOWN.isEnabled())
            this.renderLivingLabel(tnt, Modules.TNT_COUNTDOWN.label(tnt, partialTicks), x, y, z, 64);
    }
}
