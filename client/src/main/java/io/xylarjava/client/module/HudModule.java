package io.xylarjava.client.module;

import io.xylarjava.client.util.Draw;
import net.minecraft.client.gui.ScaledResolution;
import net.minecraft.client.renderer.GlStateManager;

/**
 * A module that draws something on the in-game HUD. Its position is stored as a
 * fraction of the screen so it stays in place across window sizes and GUI scales.
 */
public abstract class HudModule extends Module {

    private final float defaultX;
    private final float defaultY;

    protected HudModule(String id, String name, String description, float defaultX, float defaultY) {
        super(id, name, description);
        this.defaultX = defaultX;
        this.defaultY = defaultY;
    }

    /** Size of the element in unscaled GUI pixels. */
    public abstract int width();

    public abstract int height();

    /** Draw at (0, 0); the caller has already translated and scaled. */
    protected abstract void draw(boolean editing);

    public float scale() {
        return state != null && state.scale > 0.25f ? state.scale : 1f;
    }

    public void setScale(float scale) {
        if (state != null)
            state.scale = Math.max(0.5f, Math.min(3f, scale));
    }

    public int screenX(ScaledResolution res) {
        float x = state != null && state.x >= 0 ? state.x : defaultX;
        return clamp(Math.round(x * res.getScaledWidth()), res.getScaledWidth() - scaledWidth());
    }

    public int screenY(ScaledResolution res) {
        float y = state != null && state.y >= 0 ? state.y : defaultY;
        return clamp(Math.round(y * res.getScaledHeight()), res.getScaledHeight() - scaledHeight());
    }

    public int scaledWidth() {
        return Math.round(width() * scale());
    }

    public int scaledHeight() {
        return Math.round(height() * scale());
    }

    public void setScreenPosition(ScaledResolution res, int x, int y) {
        if (state == null)
            return;
        state.x = clamp(x, res.getScaledWidth() - scaledWidth()) / (float) res.getScaledWidth();
        state.y = clamp(y, res.getScaledHeight() - scaledHeight()) / (float) res.getScaledHeight();
    }

    public void render(ScaledResolution res, boolean editing) {
        GlStateManager.pushMatrix();
        GlStateManager.translate(screenX(res), screenY(res), 0);
        GlStateManager.scale(scale(), scale(), 1f);
        draw(editing);
        GlStateManager.popMatrix();
    }

    private static int clamp(int value, int max) {
        return Math.max(0, Math.min(value, Math.max(0, max)));
    }

    /** Common "pill" background used by the text elements. */
    protected void drawLabel(String text) {
        Draw.rect(0, 0, width(), height(), Draw.PANEL);
        mc().fontRendererObj.drawStringWithShadow(text, 4, 4, Draw.TEXT);
    }
}
