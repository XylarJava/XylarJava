package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import io.xylarjava.client.XylarClient;
import io.xylarjava.client.gui.XylarMainMenu;
import io.xylarjava.client.util.ClickTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiMainMenu;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.multiplayer.WorldClient;

@Mixin(Minecraft.class)
public abstract class MixinMinecraft {

    @Shadow
    public WorldClient theWorld;

    /** Show the XylarJava title screen wherever the game would show the vanilla one. */
    @Inject(method = "displayGuiScreen", at = @At("HEAD"), cancellable = true)
    private void xylar$replaceMainMenu(GuiScreen screen, CallbackInfo ci) {
        XylarClient.get();  // make sure the config is loaded before the first screen opens
        boolean vanillaMenu = (screen == null && this.theWorld == null) || (screen != null && screen.getClass() == GuiMainMenu.class);
        if (vanillaMenu && Modules.CUSTOM_MAIN_MENU.isEnabled()) {
            ci.cancel();
            ((Minecraft) (Object) this).displayGuiScreen(new XylarMainMenu());
        }
    }

    @Inject(method = "runTick", at = @At("RETURN"))
    private void xylar$tick(CallbackInfo ci) {
        XylarClient.get().onTick((Minecraft) (Object) this);
    }

    @Inject(method = "clickMouse", at = @At("HEAD"))
    private void xylar$leftClick(CallbackInfo ci) {
        ClickTracker.left();
    }

    @Inject(method = "rightClickMouse", at = @At("HEAD"))
    private void xylar$rightClick(CallbackInfo ci) {
        ClickTracker.right();
    }

    @Inject(method = "getLimitFramerate", at = @At("RETURN"), cancellable = true)
    private void xylar$backgroundFps(CallbackInfoReturnable<Integer> cir) {
        if (Modules.UNFOCUSED_FPS.isEnabled())
            cir.setReturnValue(Modules.UNFOCUSED_FPS.limit(cir.getReturnValue()));
    }

    @Inject(method = "toggleFullscreen", at = @At("HEAD"), cancellable = true)
    private void xylar$borderless(CallbackInfo ci) {
        if (Modules.BORDERLESS.isEnabled() && Modules.BORDERLESS.toggle((Minecraft) (Object) this))
            ci.cancel();
    }

    @Inject(method = "shutdownMinecraftApplet", at = @At("HEAD"))
    private void xylar$saveOnExit(CallbackInfo ci) {
        XylarClient.get().saveNow();
    }
}
