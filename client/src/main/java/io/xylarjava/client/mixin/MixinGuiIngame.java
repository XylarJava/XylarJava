package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import io.xylarjava.client.XylarClient;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.GuiIngame;
import net.minecraft.client.gui.ScaledResolution;
import net.minecraft.scoreboard.ScoreObjective;

@Mixin(GuiIngame.class)
public abstract class MixinGuiIngame {
    @Inject(method = "renderGameOverlay", at = @At("RETURN"))
    private void xylar$renderHud(float partialTicks, CallbackInfo ci) {
        XylarClient.get().renderHud();
    }

    /** The custom crosshair replaces the vanilla one (F3 still shows the debug axes). */
    @Inject(method = "showCrosshair", at = @At("HEAD"), cancellable = true)
    private void xylar$hideVanillaCrosshair(CallbackInfoReturnable<Boolean> cir) {
        if (Modules.CROSSHAIR.isEnabled() && !Minecraft.getMinecraft().gameSettings.showDebugInfo)
            cir.setReturnValue(false);
    }

    @Inject(method = "renderPumpkinOverlay", at = @At("HEAD"), cancellable = true)
    private void xylar$noPumpkin(ScaledResolution res, CallbackInfo ci) {
        if (Modules.NO_PUMPKIN.isEnabled())
            ci.cancel();
    }

    @Inject(method = "renderVignette", at = @At("HEAD"), cancellable = true)
    private void xylar$noVignette(float lightLevel, ScaledResolution res, CallbackInfo ci) {
        if (Modules.NO_VIGNETTE.isEnabled())
            ci.cancel();
    }

    @Inject(method = "renderPortal", at = @At("HEAD"), cancellable = true)
    private void xylar$noPortal(float timeInPortal, ScaledResolution res, CallbackInfo ci) {
        if (Modules.NO_PORTAL.isEnabled())
            ci.cancel();
    }

    @Inject(method = "renderScoreboard", at = @At("HEAD"), cancellable = true)
    private void xylar$hideScoreboard(ScoreObjective objective, ScaledResolution res, CallbackInfo ci) {
        if (Modules.SCOREBOARD.isEnabled() && Modules.SCOREBOARD.hide.on())
            ci.cancel();
    }

    /** The second string drawn per scoreboard line is the red score number. */
    @Redirect(method = "renderScoreboard", at = @At(value = "INVOKE", target = "Lnet/minecraft/client/gui/FontRenderer;drawString(Ljava/lang/String;III)I", ordinal = 1))
    private int xylar$scoreNumbers(FontRenderer font, String text, int x, int y, int color) {
        if (Modules.SCOREBOARD.isEnabled() && Modules.SCOREBOARD.numbers.on())
            return 0;
        return font.drawString(text, x, y, color);
    }

    @Inject(method = "renderBossHealth", at = @At("HEAD"), cancellable = true)
    private void xylar$hideBossBar(CallbackInfo ci) {
        if (Modules.HIDE_BOSS_BAR.isEnabled())
            ci.cancel();
    }

    @Inject(method = "displayTitle", at = @At("HEAD"), cancellable = true)
    private void xylar$hideTitles(String title, String subTitle, int fadeIn, int stay, int fadeOut, CallbackInfo ci) {
        if (Modules.HIDE_TITLES.isEnabled())
            ci.cancel();
    }
}
