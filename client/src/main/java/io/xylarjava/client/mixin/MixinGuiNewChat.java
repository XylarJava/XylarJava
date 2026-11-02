package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.injection.Constant;
import org.spongepowered.asm.mixin.injection.ModifyConstant;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Hooks;
import io.xylarjava.client.Modules;
import net.minecraft.client.gui.GuiNewChat;
import net.minecraft.util.IChatComponent;

@Mixin(GuiNewChat.class)
public abstract class MixinGuiNewChat {
    /** Vanilla keeps 100 lines (drawn and history); allow more. */
    @ModifyConstant(method = "setChatLine", constant = @Constant(intValue = 100))
    private int xylar$chatHistory(int lines) {
        return Modules.CHAT_HISTORY.isEnabled() ? Modules.CHAT_HISTORY.lines() : lines;
    }

    @Inject(method = "printChatMessage", at = @At("HEAD"), cancellable = true)
    private void xylar$chat(IChatComponent message, CallbackInfo ci) {
        if (Hooks.onChatMessage((GuiNewChat) (Object) this, message))
            ci.cancel();
    }
}
