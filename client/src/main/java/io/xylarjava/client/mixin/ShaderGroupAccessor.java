package io.xylarjava.client.mixin;

import java.util.List;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

import net.minecraft.client.shader.Shader;
import net.minecraft.client.shader.ShaderGroup;

/** The passes of a loaded shader, so their uniforms (blur radius, trail length) can change every frame. */
@Mixin(ShaderGroup.class)
public interface ShaderGroupAccessor {
    @Accessor("listShaders")
    List<Shader> xylar$passes();
}
