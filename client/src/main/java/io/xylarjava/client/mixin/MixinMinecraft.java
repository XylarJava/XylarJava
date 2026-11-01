package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

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
        boolean vanillaMenu = (screen == null && this.theWorld == null) || (screen != null && screen.getClass() == GuiMainMenu.class);
        if (vanillaMenu && XylarClient.get().config().customMainMenu) {
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

    @Inject(method = "shutdownMinecraftApplet", at = @At("HEAD"))
    private void xylar$saveOnExit(CallbackInfo ci) {
        XylarClient.get().saveNow();
    }
}
