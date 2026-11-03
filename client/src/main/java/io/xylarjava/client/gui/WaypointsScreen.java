package io.xylarjava.client.gui;

import java.io.IOException;
import java.util.List;

import org.lwjgl.input.Keyboard;
import org.lwjgl.input.Mouse;

import io.xylarjava.client.Modules;
import io.xylarjava.client.XylarClient;
import io.xylarjava.client.module.impl.WaypointsModule;
import io.xylarjava.client.setting.ColorSetting;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.gui.GuiButton;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.GuiTextField;

/** Waypoints of the world you're in: add, rename by adding, recolour, hide and delete. */
public class WaypointsScreen extends GuiScreen {

    private static final int ADD = 1;
    private static final int CLOSE = 2;
    private static final int ROW = 22;

    private GuiTextField name;
    private int panelX, panelY, panelW, panelH;
    private int listY, listH;
    private float scroll;

    @Override
    public void initGui() {
        Keyboard.enableRepeatEvents(true);
        panelW = Math.min(380, this.width - 20);
        panelH = Math.min(260, this.height - 20);
        panelX = (this.width - panelW) / 2;
        panelY = (this.height - panelH) / 2;
        listY = panelY + 60;
        listH = panelY + panelH - 34 - listY;
        String typed = name == null ? "" : name.getText();
        name = new GuiTextField(0, this.fontRendererObj, panelX + 14, panelY + 34, panelW - 120, 14);
        name.setMaxStringLength(32);
        name.setEnableBackgroundDrawing(false);
        name.setText(typed);
        name.setFocused(true);
        this.buttonList.clear();
        this.buttonList.add(new XButton(ADD, panelX + panelW - 96, panelY + 30, 86, 20, "Add here", true));
        this.buttonList.add(new XButton(CLOSE, panelX + panelW - 80, panelY + panelH - 28, 70, 20, "Close"));
    }

    @Override
    public void onGuiClosed() {
        Keyboard.enableRepeatEvents(false);
        XylarClient.get().markDirty();
    }

    private List<WaypointsModule.Waypoint> waypoints() {
        return Modules.WAYPOINTS.here();
    }

    private float maxScroll() {
        return Math.max(0, waypoints().size() * ROW - listH);
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        if (this.mc.theWorld != null)
            this.drawGradientRect(0, 0, this.width, this.height, 0x66000000, 0x99000000);
        else
            this.drawDefaultBackground();
        Draw.softRect(panelX, panelY, panelW, panelH, 0xF20E1016);
        Draw.outline(panelX, panelY, panelW, panelH, Draw.BORDER);
        this.fontRendererObj.drawStringWithShadow("Waypoints", panelX + 12, panelY + 12, Draw.TEXT);
        String where = WaypointsModule.currentWorld().replaceFirst("^(sp|mp):", "");
        this.fontRendererObj.drawString(this.fontRendererObj.trimStringToWidth(where, panelW - 100),
                panelX + 12 + this.fontRendererObj.getStringWidth("Waypoints") + 6, panelY + 12, Draw.ACCENT);

        Draw.softRect(panelX + 10, panelY + 30, panelW - 112, 20, 0xFF161922);
        Draw.outline(panelX + 10, panelY + 30, panelW - 112, 20, name.isFocused() ? 0x88FF9A3C : 0x22FFFFFF);
        if (name.getText().isEmpty())
            this.fontRendererObj.drawString("Name (optional)", panelX + 14, panelY + 36, Draw.MUTED);
        name.xPosition = panelX + 14;
        name.yPosition = panelY + 36;
        name.drawTextBox();

        List<WaypointsModule.Waypoint> list = waypoints();
        scroll = Math.max(0, Math.min(maxScroll(), scroll));
        Draw.scissor(this.mc, panelX + 1, listY, panelW - 2, listH);
        int y = listY - Math.round(scroll);
        for (WaypointsModule.Waypoint wp : list) {
            if (y + ROW >= listY && y <= listY + listH)
                drawRow(wp, y, mouseX, mouseY);
            y += ROW;
        }
        Draw.endScissor();
        if (list.isEmpty())
            Draw.centeredText("No waypoints here yet. Press B in game or Add here.", panelX + panelW / 2, listY + 20, Draw.MUTED);

        super.drawScreen(mouseX, mouseY, partialTicks);
    }

    private void drawRow(WaypointsModule.Waypoint wp, int y, int mouseX, int mouseY) {
        int x = panelX + 10;
        int w = panelW - 20;
        boolean hovered = Draw.inside(mouseX, mouseY, x, y, w, ROW - 2) && mouseY >= listY && mouseY < listY + listH;
        Draw.softRect(x, y, w, ROW - 2, hovered ? 0xFF1C202B : 0xFF161922);
        Draw.rect(x + 6, y + 6, 8, 8, wp.color);
        Draw.outline(x + 5, y + 5, 10, 10, 0x55FFFFFF);
        int textColor = wp.visible ? Draw.TEXT : Draw.MUTED;
        this.fontRendererObj.drawString(this.fontRendererObj.trimStringToWidth(wp.name, 110), x + 20, y + 6, textColor);
        String info = wp.x + " " + wp.y + " " + wp.z + (this.mc.thePlayer != null ? "  " + Math.round(wp.distanceTo(this.mc.thePlayer)) + "m" : "");
        this.fontRendererObj.drawString(info, x + 136, y + 6, Draw.MUTED);
        drawLink(wp.visible ? "Hide" : "Show", x + w - 70, y + 6, mouseX, mouseY);
        drawLink("Delete", x + w - 38, y + 6, mouseX, mouseY);
    }

    private void drawLink(String text, int x, int y, int mouseX, int mouseY) {
        boolean over = Draw.inside(mouseX, mouseY, x - 2, y - 4, this.fontRendererObj.getStringWidth(text) + 4, 16);
        this.fontRendererObj.drawString(text, x, y, over ? Draw.ACCENT : Draw.TEXT);
    }

    @Override
    protected void mouseClicked(int mouseX, int mouseY, int button) throws IOException {
        super.mouseClicked(mouseX, mouseY, button);
        name.mouseClicked(mouseX, mouseY, button);
        if (mouseY < listY || mouseY >= listY + listH)
            return;
        int index = (mouseY - listY + Math.round(scroll)) / ROW;
        List<WaypointsModule.Waypoint> list = waypoints();
        if (index < 0 || index >= list.size())
            return;
        WaypointsModule.Waypoint wp = list.get(index);
        int x = panelX + 10;
        int w = panelW - 20;
        if (mouseX >= x + w - 72 && mouseX < x + w - 42)
            wp.visible = !wp.visible;
        else if (mouseX >= x + w - 40 && mouseX < x + w)
            Modules.WAYPOINTS.remove(wp);
        else if (mouseX >= x + 4 && mouseX < x + 17)
            wp.color = next(wp.color);
        else
            return;
        XylarClient.get().markDirty();
    }

    private static int next(int color) {
        int[] palette = ColorSetting.PALETTE;
        for (int i = 0; i < palette.length; i++) {
            if (palette[i] == color)
                return palette[(i + 1) % palette.length];
        }
        return palette[0];
    }

    @Override
    public void handleMouseInput() throws IOException {
        super.handleMouseInput();
        int wheel = Mouse.getEventDWheel();
        if (wheel != 0)
            scroll -= Integer.signum(wheel) * ROW;
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) throws IOException {
        if (keyCode == Keyboard.KEY_RETURN) {
            actionPerformed((GuiButton) this.buttonList.get(0));
            return;
        }
        if (name.textboxKeyTyped(typedChar, keyCode))
            return;
        super.keyTyped(typedChar, keyCode);
    }

    @Override
    protected void actionPerformed(GuiButton button) {
        if (button.id == ADD) {
            Modules.WAYPOINTS.addHere(name.getText());
            name.setText("");
        } else if (button.id == CLOSE) {
            this.mc.displayGuiScreen(null);
        }
    }

    @Override
    public void updateScreen() {
        name.updateCursorCounter();
    }

    @Override
    public boolean doesGuiPauseGame() {
        return false;
    }
}
