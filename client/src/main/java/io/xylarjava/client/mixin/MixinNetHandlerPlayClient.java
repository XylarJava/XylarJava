package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.network.NetHandlerPlayClient;
import net.minecraft.network.play.server.S03PacketTimeUpdate;

@Mixin(NetHandlerPlayClient.class)
public abstract class MixinNetHandlerPlayClient {
    /** With Time Changer on, the server's time updates are ignored (only on your screen). */
    @Inject(method = "handleTimeUpdate", at = @At("HEAD"), cancellable = true)
    private void xylar$timeChanger(S03PacketTimeUpdate packet, CallbackInfo ci) {
        if (Modules.TIME_CHANGER.isEnabled())
            ci.cancel();
    }
}
