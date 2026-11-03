package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

import net.minecraft.client.renderer.EntityRenderer;
import net.minecraft.util.ResourceLocation;

/** Lets Menu Blur and Motion Blur load their post-processing shaders. */
@Mixin(EntityRenderer.class)
public interface EntityRendererAccessor {
    @Invoker("loadShader")
    void xylar$loadShader(ResourceLocation location);
}
