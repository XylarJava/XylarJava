package io.xylarjava.client.gui;

import java.io.IOException;

import org.lwjgl.input.Mouse;

import io.xylarjava.client.XylarClient;
import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.ScaledResolution;

/** Drag HUD elements around; scroll over one to resize it. Esc goes back. */
public class HudEditorScreen extends GuiScreen {

    private final GuiScreen parent;
    private HudModule dragging;
    private int grabX;
    private int grabY;

    public HudEditorScreen(GuiScreen parent) {
        this.parent = parent;
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        this.drawGradientRect(0, 0, this.width, this.height, 0x55000000, 0x77000000);
        // Light grid to help line things up.
        for (int x = 0; x < this.width; x += 20)
            Draw.rect(x, 0, 1, this.height, 0x0FFFFFFF);
        for (int y = 0; y < this.height; y += 20)
            Draw.rect(0, y, this.width, 1, 0x0FFFFFFF);

        ScaledResolution res = new ScaledResolution(this.mc);
        if (dragging != null)
            dragging.setScreenPosition(res, mouseX - grabX, mouseY - grabY);

        for (Module module : XylarClient.get().modules()) {
            if (!(module instanceof HudModule) || !module.isEnabled())
                continue;
            HudModule hud = (HudModule) module;
            hud.render(res, true);
            int x = hud.screenX(res);
            int y = hud.screenY(res);
            boolean hovered = Draw.inside(mouseX, mouseY, x, y, hud.scaledWidth(), hud.scaledHeight());
            Draw.outline(x - 1, y - 1, hud.scaledWidth() + 2, hud.scaledHeight() + 2,
                         hud == dragging || hovered ? Draw.ACCENT : 0x66FFFFFF);
            if (hovered && dragging == null)
                this.fontRendererObj.drawStringWithShadow(hud.name(), x, Math.max(0, y - 10), Draw.ACCENT);
        }

        Draw.centeredText("Drag to move  -  Scroll to resize  -  Esc when you're done", this.width / 2, 6, Draw.TEXT);
        super.drawScreen(mouseX, mouseY, partialTicks);
    }

    private HudModule elementAt(int mouseX, int mouseY) {
        ScaledResolution res = new ScaledResolution(this.mc);
        HudModule found = null;
        for (Module module : XylarClient.get().modules()) {
            if (!(module instanceof HudModule) || !module.isEnabled())
                continue;
            HudModule hud = (HudModule) module;
            if (Draw.inside(mouseX, mouseY, hud.screenX(res), hud.screenY(res), hud.scaledWidth(), hud.scaledHeight()))
                found = hud;  // last drawn is on top
        }
        return found;
    }

    @Override
    protected void mouseClicked(int mouseX, int mouseY, int mouseButton) throws IOException {
        if (mouseButton == 0) {
            HudModule hud = elementAt(mouseX, mouseY);
            if (hud != null) {
                ScaledResolution res = new ScaledResolution(this.mc);
                dragging = hud;
                grabX = mouseX - hud.screenX(res);
                grabY = mouseY - hud.screenY(res);
                return;
            }
        }
        super.mouseClicked(mouseX, mouseY, mouseButton);
    }

    @Override
    protected void mouseReleased(int mouseX, int mouseY, int state) {
        if (dragging != null) {
            dragging = null;
            XylarClient.get().markDirty();
        }
        super.mouseReleased(mouseX, mouseY, state);
    }

    @Override
    public void handleMouseInput() throws IOException {
        super.handleMouseInput();
        int wheel = Mouse.getEventDWheel();
        if (wheel == 0)
            return;
        int mouseX = Mouse.getEventX() * this.width / this.mc.displayWidth;
        int mouseY = this.height - Mouse.getEventY() * this.height / this.mc.displayHeight - 1;
        HudModule hud = elementAt(mouseX, mouseY);
        if (hud != null) {
            hud.setScale(hud.scale() + (wheel > 0 ? 0.1f : -0.1f));
            XylarClient.get().markDirty();
        }
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) throws IOException {
        if (keyCode == 1) {  // Esc
            this.mc.displayGuiScreen(parent);
            return;
        }
        super.keyTyped(typedChar, keyCode);
    }

    @Override
    public void onGuiClosed() {
        XylarClient.get().saveNow();
    }

    @Override
    public boolean doesGuiPauseGame() {
        return false;
    }
}
