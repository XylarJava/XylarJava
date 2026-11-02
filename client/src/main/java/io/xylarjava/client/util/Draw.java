package io.xylarjava.client.util;

import org.lwjgl.opengl.GL11;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.ScaledResolution;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;
import net.minecraft.client.renderer.GlStateManager;

/** Small drawing helpers and the client's colour palette (ARGB). */
public final class Draw {

    public static final int ACCENT = 0xFFFF9A3C;
    public static final int ACCENT_DARK = 0xFFD9771F;
    public static final int TEXT = 0xFFE9EBF3;
    public static final int MUTED = 0xFF8E94A8;
    public static final int PANEL = 0x99101218;
    public static final int PANEL_SOLID = 0xE6141720;
    public static final int PANEL_HOVER = 0xE6232736;
    public static final int BORDER = 0x33FFFFFF;
    public static final int PRESSED = 0xCCFFFFFF;

    private Draw() {}

    public static void rect(int x, int y, int w, int h, int color) {
        Gui.drawRect(x, y, x + w, y + h, color);
    }

    /** 1px outline. */
    public static void outline(int x, int y, int w, int h, int color) {
        rect(x, y, w, 1, color);
        rect(x, y + h - 1, w, 1, color);
        rect(x, y + 1, 1, h - 2, color);
        rect(x + w - 1, y + 1, 1, h - 2, color);
    }

    /** Rectangle with the corners cut by one pixel, which reads as slightly rounded at GUI scale. */
    public static void softRect(int x, int y, int w, int h, int color) {
        rect(x + 1, y, w - 2, h, color);
        rect(x, y + 1, 1, h - 2, color);
        rect(x + w - 1, y + 1, 1, h - 2, color);
    }

    public static void centeredText(String text, int centerX, int y, int color) {
        FontRenderer font = Minecraft.getMinecraft().fontRendererObj;
        font.drawStringWithShadow(text, centerX - font.getStringWidth(text) / 2f, y, color);
    }

    /**
     * Text drawn with the game's own font, scaled up (used for the big XYLARJAVA title).
     * Extra letter spacing is added between characters.
     */
    public static void bigText(String text, int centerX, int y, float scale, int spacing, int color) {
        FontRenderer font = Minecraft.getMinecraft().fontRendererObj;
        int width = 0;
        for (int i = 0; i < text.length(); i++)
            width += font.getCharWidth(text.charAt(i)) + (i < text.length() - 1 ? spacing : 0);

        GlStateManager.pushMatrix();
        GlStateManager.translate(centerX - width * scale / 2f, y, 0);
        GlStateManager.scale(scale, scale, 1f);
        int x = 0;
        for (int i = 0; i < text.length(); i++) {
            String ch = String.valueOf(text.charAt(i));
            font.drawStringWithShadow(ch, x, 0, color);
            x += font.getCharWidth(text.charAt(i)) + spacing;
        }
        GlStateManager.popMatrix();
    }

    /** Clip drawing to a rectangle in GUI coordinates until {@link #endScissor()}. */
    public static void scissor(Minecraft mc, int x, int y, int w, int h) {
        ScaledResolution res = new ScaledResolution(mc);
        int scale = res.getScaleFactor();
        GL11.glEnable(GL11.GL_SCISSOR_TEST);
        GL11.glScissor(x * scale, mc.displayHeight - (y + h) * scale, Math.max(0, w * scale), Math.max(0, h * scale));
    }

    public static void endScissor() {
        GL11.glDisable(GL11.GL_SCISSOR_TEST);
    }

    /** Blend two ARGB colours. */
    public static int blend(int from, int to, float t) {
        t = Math.max(0f, Math.min(1f, t));
        int out = 0;
        for (int shift = 24; shift >= 0; shift -= 8) {
            int a = (from >>> shift) & 0xFF;
            int b = (to >>> shift) & 0xFF;
            out |= ((int) (a + (b - a) * t) & 0xFF) << shift;
        }
        return out;
    }

    public static boolean inside(int mouseX, int mouseY, int x, int y, int w, int h) {
        return mouseX >= x && mouseY >= y && mouseX < x + w && mouseY < y + h;
    }
}
