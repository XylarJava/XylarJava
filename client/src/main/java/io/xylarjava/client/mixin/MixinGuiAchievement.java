package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.gui.achievement.GuiAchievement;

@Mixin(GuiAchievement.class)
public abstract class MixinGuiAchievement {
    @Inject(method = "updateAchievementWindow", at = @At("HEAD"), cancellable = true)
    private void xylar$noAchievements(CallbackInfo ci) {
        if (Modules.NO_ACHIEVEMENTS.isEnabled())
            ci.cancel();
    }
}
