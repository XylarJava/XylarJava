package io.xylarjava.client.mixin;

import java.io.File;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.client.shader.Framebuffer;
import net.minecraft.util.IChatComponent;
import net.minecraft.util.ScreenShotHelper;

@Mixin(ScreenShotHelper.class)
public abstract class MixinScreenShotHelper {
    /** Screenshot Helper: copy the picture and add an "open folder" link to the chat message. */
    @Inject(method = "saveScreenshot(Ljava/io/File;Ljava/lang/String;IILnet/minecraft/client/shader/Framebuffer;)Lnet/minecraft/util/IChatComponent;",
            at = @At("RETURN"), cancellable = true)
    private static void xylar$screenshot(File gameDir, String name, int width, int height, Framebuffer buffer,
                                         CallbackInfoReturnable<IChatComponent> cir) {
        if (Modules.SCREENSHOTS.isEnabled())
            cir.setReturnValue(Modules.SCREENSHOTS.saved(gameDir, cir.getReturnValue()));
    }
}
