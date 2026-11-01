package io.xylarjava.client.gui;

import io.xylarjava.client.util.Draw;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiButton;

/** Flat button in the XylarJava style, with a short hover fade. */
public class XButton extends GuiButton {

    private final boolean accent;
    private float hover;
    private long lastFrame = System.currentTimeMillis();

    public XButton(int id, int x, int y, int width, int height, String text) {
        this(id, x, y, width, height, text, false);
    }

    public XButton(int id, int x, int y, int width, int height, String text, boolean accent) {
        super(id, x, y, width, height, text);
        this.accent = accent;
    }

    @Override
    public void drawButton(Minecraft mc, int mouseX, int mouseY) {
        if (!this.visible)
            return;
        this.hovered = Draw.inside(mouseX, mouseY, this.xPosition, this.yPosition, this.width, this.height);

        long now = System.currentTimeMillis();
        float step = Math.min(1f, (now - lastFrame) / 120f);
        lastFrame = now;
        hover += ((this.hovered && this.enabled ? 1f : 0f) - hover) * step;

        int background;
        int text;
        if (!this.enabled) {
            background = 0x88101218;
            text = Draw.MUTED;
        } else if (accent) {
            background = blend(Draw.ACCENT_DARK, Draw.ACCENT, hover);
            text = 0xFF1A1006;
        } else {
            background = blend(Draw.PANEL_SOLID, Draw.PANEL_HOVER, hover);
            text = Draw.TEXT;
        }

        Draw.softRect(this.xPosition, this.yPosition, this.width, this.height, background);
        if (!accent && this.enabled) {
            // Accent line under the hovered button.
            int lineWidth = Math.round((this.width - 8) * hover);
            if (lineWidth > 0)
                Draw.rect(this.xPosition + (this.width - lineWidth) / 2, this.yPosition + this.height - 2, lineWidth, 1, Draw.ACCENT);
        }
        Draw.centeredText(this.displayString, this.xPosition + this.width / 2, this.yPosition + (this.height - 8) / 2, text);
    }

    private static int blend(int from, int to, float t) {
        t = Math.max(0f, Math.min(1f, t));
        int a = (int) (((from >>> 24) & 0xFF) + (((to >>> 24) & 0xFF) - ((from >>> 24) & 0xFF)) * t);
        int r = (int) (((from >> 16) & 0xFF) + (((to >> 16) & 0xFF) - ((from >> 16) & 0xFF)) * t);
        int g = (int) (((from >> 8) & 0xFF) + (((to >> 8) & 0xFF) - ((from >> 8) & 0xFF)) * t);
        int b = (int) ((from & 0xFF) + ((to & 0xFF) - (from & 0xFF)) * t);
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
}
