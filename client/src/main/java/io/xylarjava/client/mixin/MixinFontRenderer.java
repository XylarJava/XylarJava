package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.ModifyVariable;

import io.xylarjava.client.Hooks;
import net.minecraft.client.gui.FontRenderer;

@Mixin(FontRenderer.class)
public abstract class MixinFontRenderer {
    /** Name Hider: swap your name before any text is drawn. */
    @ModifyVariable(method = "renderString", at = @At("HEAD"), index = 1)
    private String xylar$hideName(String text) {
        return Hooks.hideName(text);
    }
}
