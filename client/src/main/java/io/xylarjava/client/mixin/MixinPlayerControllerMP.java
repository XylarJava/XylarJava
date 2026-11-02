package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Hooks;
import net.minecraft.client.multiplayer.PlayerControllerMP;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.EntityPlayer;

@Mixin(PlayerControllerMP.class)
public abstract class MixinPlayerControllerMP {
    @Inject(method = "attackEntity", at = @At("HEAD"))
    private void xylar$onAttack(EntityPlayer player, Entity target, CallbackInfo ci) {
        Hooks.onAttack(player, target);
    }
}
