package io.xylarjava.client.util;

import io.xylarjava.client.Modules;
import io.xylarjava.client.mixin.EntityRendererAccessor;
import io.xylarjava.client.mixin.ShaderGroupAccessor;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.EntityRenderer;
import net.minecraft.client.renderer.OpenGlHelper;
import net.minecraft.client.shader.Shader;
import net.minecraft.client.shader.ShaderGroup;
import net.minecraft.client.shader.ShaderUniform;
import net.minecraft.util.ResourceLocation;

/**
 * The game draws one post-processing shader at a time. This picks which of ours it should be
 * (the menu blur while a menu is open, motion blur otherwise) and never replaces a shader the
 * game loaded itself, like the creeper view when spectating one.
 */
public final class ShaderEffects {

    private enum Effect {
        NONE(null, null),
        MENU_BLUR(new ResourceLocation("shaders/post/blur.json"), "Radius"),
        MOTION_BLUR(new ResourceLocation("shaders/post/xylarjava_motion_blur.json"), "BlendFactor");

        final ResourceLocation location;
        final String uniform;
        boolean failed;

        Effect(ResourceLocation location, String uniform) {
            this.location = location;
            this.uniform = uniform;
        }
    }

    private static ShaderGroup ours;
    private static Effect oursEffect = Effect.NONE;
    private static Object blurScreen;
    private static long blurStart;

    private ShaderEffects() {}

    /** Before every frame. */
    public static void update(EntityRenderer renderer) {
        Minecraft mc = Minecraft.getMinecraft();
        Effect wanted = Effect.NONE;
        if (OpenGlHelper.shadersSupported && mc.theWorld != null) {
            if (Modules.MENU_BLUR.wants(mc.currentScreen))
                wanted = Effect.MENU_BLUR;
            else if (Modules.MOTION_BLUR.isEnabled() && mc.currentScreen == null)
                wanted = Effect.MOTION_BLUR;
        }
        if (wanted.failed)
            wanted = Effect.NONE;

        if (wanted == Effect.MENU_BLUR && mc.currentScreen != blurScreen) {
            blurScreen = mc.currentScreen;
            blurStart = System.currentTimeMillis();
        } else if (wanted != Effect.MENU_BLUR) {
            blurScreen = null;
        }

        ShaderGroup current = renderer.getShaderGroup();
        boolean currentIsOurs = current != null && current == ours;
        if (current != null && !currentIsOurs)
            return;  // the game's own shader wins

        if (wanted == Effect.NONE) {
            if (currentIsOurs)
                renderer.stopUseShader();
            ours = null;
            oursEffect = Effect.NONE;
            return;
        }

        if (!currentIsOurs || oursEffect != wanted) {
            if (currentIsOurs)
                renderer.stopUseShader();
            ((EntityRendererAccessor) renderer).xylar$loadShader(wanted.location);
            ours = renderer.getShaderGroup();
            oursEffect = ours == null ? Effect.NONE : wanted;
            if (ours == null) {
                wanted.failed = true;  // no shader support: don't try every frame
                System.err.println("[XylarJava] " + wanted + " shader could not be loaded; turning it off for this session.");
                return;
            }
        }

        float value = wanted == Effect.MENU_BLUR ? Modules.MENU_BLUR.radius(System.currentTimeMillis() - blurStart) : Modules.MOTION_BLUR.blend();
        setUniform(ours, wanted.uniform, value);
    }

    private static void setUniform(ShaderGroup group, String name, float value) {
        for (Shader pass : ((ShaderGroupAccessor) group).xylar$passes()) {
            ShaderUniform uniform = pass.getShaderManager().getShaderUniform(name);
            if (uniform != null)
                uniform.set(value);
        }
    }
}
