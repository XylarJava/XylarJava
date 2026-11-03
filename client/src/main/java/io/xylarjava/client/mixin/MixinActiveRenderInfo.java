package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Redirect;

import io.xylarjava.client.Modules;
import net.minecraft.client.renderer.ActiveRenderInfo;
import net.minecraft.entity.player.EntityPlayer;

@Mixin(ActiveRenderInfo.class)
public abstract class MixinActiveRenderInfo {
    /** Particles face the Freelook camera, not where the player is looking. */
    @Redirect(method = "updateRenderInfo", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/player/EntityPlayer;rotationYaw:F"))
    private static float xylar$freelookYaw(EntityPlayer player) {
        return Modules.FREELOOK.yaw(player, player.rotationYaw);
    }

    @Redirect(method = "updateRenderInfo", at = @At(value = "FIELD", target = "Lnet/minecraft/entity/player/EntityPlayer;rotationPitch:F"))
    private static float xylar$freelookPitch(EntityPlayer player) {
        return Modules.FREELOOK.pitch(player, player.rotationPitch);
    }
}
