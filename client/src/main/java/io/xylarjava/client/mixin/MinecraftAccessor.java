package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

import net.minecraft.client.Minecraft;

/** Lets borderless fullscreen resize the game after changing the window. */
@Mixin(Minecraft.class)
public interface MinecraftAccessor {
    @Invoker("resize")
    void xylar$resize(int width, int height);
}
