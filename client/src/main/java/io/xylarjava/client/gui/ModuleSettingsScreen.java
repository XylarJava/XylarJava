package io.xylarjava.client.gui;

import java.io.IOException;

import org.lwjgl.input.Keyboard;
import org.lwjgl.input.Mouse;

import io.xylarjava.client.XylarClient;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ColorSetting;
import io.xylarjava.client.setting.KeySetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;
import io.xylarjava.client.setting.Setting;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.audio.PositionedSoundRecord;
import net.minecraft.client.gui.GuiButton;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.util.ResourceLocation;

/** Options of one module: switches, sliders, choices, colours and keys. */
public class ModuleSettingsScreen extends GuiScreen {

    private static final int BACK = 1;
    private static final int RESET = 2;
    private static final int ROW = 22;

    private final GuiScreen parent;
    private final Module module;
    private int panelX, panelY, panelW, panelH;
    private int listY, listH;
    private float scroll;
    private NumberSetting dragging;
    private KeySetting listening;

    public ModuleSettingsScreen(GuiScreen parent, Module module) {
        this.parent = parent;
        this.module = module;
    }

    @Override
    public void initGui() {
        panelW = Math.min(360, this.width - 20);
        int wanted = 70 + (module.settings().size() + 1) * ROW + 34;
        panelH = Math.min(wanted, this.height - 20);
        panelX = (this.width - panelW) / 2;
        panelY = (this.height - panelH) / 2;
        listY = panelY + 52;
        listH = panelY + panelH - 34 - listY;
        this.buttonList.clear();
        int y = panelY + panelH - 28;
        this.buttonList.add(new XButton(RESET, panelX + 10, y, 110, 20, "Reset to defaults"));
        this.buttonList.add(new XButton(BACK, panelX + panelW - 10 - 70, y, 70, 20, "Back", true));
    }

    private int rows() {
        return module.settings().size() + 1;  // first row: the module's own switch
    }

    private float maxScroll() {
        return Math.max(0, rows() * ROW - listH);
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        if (this.mc.theWorld != null)
            this.drawGradientRect(0, 0, this.width, this.height, 0x99000000, 0xBB000000);
        else
            this.drawDefaultBackground();

        if (dragging != null) {
            if (!Mouse.isButtonDown(0)) {
                dragging = null;
                XylarClient.get().markDirty();
            } else {
                dragging.setFraction((mouseX - sliderX()) / (float) sliderW());
            }
        }

        Draw.softRect(panelX, panelY, panelW, panelH, 0xF20E1016);
        Draw.outline(panelX, panelY, panelW, panelH, Draw.BORDER);
        this.fontRendererObj.drawStringWithShadow(module.name(), panelX + 12, panelY + 12, Draw.TEXT);
        this.fontRendererObj.drawString(module.category().title(), panelX + 12 + this.fontRendererObj.getStringWidth(module.name()) + 6,
                panelY + 12, Draw.ACCENT);
        this.fontRendererObj.drawString(this.fontRendererObj.trimStringToWidth(module.description(), panelW - 24), panelX + 12, panelY + 28, Draw.MUTED);
        Draw.rect(panelX + 10, panelY + 44, panelW - 20, 1, 0x22FFFFFF);

        Draw.scissor(this.mc, panelX + 1, listY, panelW - 2, listH);
        int y = listY - Math.round(scroll);
        drawRow("Enabled", null, y, mouseX, mouseY);
        for (Setting<?> setting : module.settings()) {
            y += ROW;
            drawRow(setting.name(), setting, y, mouseX, mouseY);
        }
        Draw.endScissor();

        if (maxScroll() > 0) {
            int barH = Math.max(16, listH * listH / (rows() * ROW));
            int barY = listY + Math.round((listH - barH) * (scroll / maxScroll()));
            Draw.rect(panelX + panelW - 5, barY, 2, barH, Draw.ACCENT);
        }
        super.drawScreen(mouseX, mouseY, partialTicks);
    }

    private int controlX() {
        return panelX + panelW - 12 - 130;
    }

    private int sliderX() {
        return controlX();
    }

    private int sliderW() {
        return 96;
    }

    private void drawRow(String label, Setting<?> setting, int y, int mouseX, int mouseY) {
        if (y + ROW < listY || y > listY + listH)
            return;
        boolean hovered = Draw.inside(mouseX, mouseY, panelX + 6, y, panelW - 12, ROW - 2) && Draw.inside(mouseX, mouseY, panelX, listY, panelW, listH);
        if (hovered)
            Draw.softRect(panelX + 6, y, panelW - 12, ROW - 2, 0xFF181B24);
        this.fontRendererObj.drawString(label, panelX + 14, y + 6, Draw.TEXT);
        int cx = controlX();
        int right = panelX + panelW - 14;

        if (setting == null || setting instanceof BoolSetting) {
            boolean on = setting == null ? module.isEnabled() : ((BoolSetting) setting).on();
            int sx = right - 24;
            Draw.softRect(sx, y + 5, 24, 11, on ? Draw.ACCENT : 0xFF2C3142);
            Draw.rect(on ? sx + 14 : sx + 2, y + 7, 8, 7, on ? 0xFF1A1006 : Draw.MUTED);
        } else if (setting instanceof NumberSetting) {
            NumberSetting n = (NumberSetting) setting;
            int sx = sliderX();
            Draw.rect(sx, y + 9, sliderW(), 3, 0xFF2C3142);
            int filled = Math.round(sliderW() * n.fraction());
            Draw.rect(sx, y + 9, filled, 3, Draw.ACCENT);
            Draw.softRect(sx + filled - 2, y + 6, 5, 9, n == dragging ? 0xFFFFFFFF : 0xFFE9EBF3);
            String value = n.display();
            this.fontRendererObj.drawString(value, right - this.fontRendererObj.getStringWidth(value), y + 6, Draw.MUTED);
        } else if (setting instanceof ModeSetting) {
            String value = setting.display();
            Draw.softRect(cx, y + 2, right - cx, ROW - 6, 0xFF1E222E);
            this.fontRendererObj.drawString("<", cx + 5, y + 6, Draw.MUTED);
            this.fontRendererObj.drawString(">", right - 10, y + 6, Draw.MUTED);
            Draw.centeredText(value, (cx + right) / 2, y + 6, Draw.TEXT);
        } else if (setting instanceof ColorSetting) {
            ColorSetting c = (ColorSetting) setting;
            String value = c.display();
            Draw.softRect(right - 14, y + 4, 14, 13, 0xFF000000);
            Draw.rect(right - 13, y + 5, 12, 11, c.argb() | 0xFF000000);
            this.fontRendererObj.drawString(value, right - 20 - this.fontRendererObj.getStringWidth(value), y + 6, Draw.MUTED);
        } else if (setting instanceof KeySetting) {
            String value = setting == listening ? "Press a key..." : setting.display();
            Draw.softRect(cx, y + 2, right - cx, ROW - 6, setting == listening ? 0xFF3A2A18 : 0xFF1E222E);
            Draw.centeredText(value, (cx + right) / 2, y + 6, setting == listening ? Draw.ACCENT : Draw.TEXT);
        }
    }

    private void click() {
        this.mc.getSoundHandler().playSound(PositionedSoundRecord.create(new ResourceLocation("gui.button.press"), 1.0F));
    }

    @Override
    protected void mouseClicked(int mouseX, int mouseY, int mouseButton) throws IOException {
        listening = null;
        if (Draw.inside(mouseX, mouseY, panelX, listY, panelW, listH)) {
            int index = (mouseY - listY + Math.round(scroll)) / ROW;
            if (index == 0) {
                module.toggle();
                click();
            } else if (index - 1 < module.settings().size()) {
                Setting<?> setting = module.settings().get(index - 1);
                int cx = controlX();
                int right = panelX + panelW - 14;
                if (setting instanceof BoolSetting) {
                    ((BoolSetting) setting).toggle();
                    click();
                } else if (setting instanceof NumberSetting) {
                    if (mouseX >= sliderX() - 4 && mouseX <= sliderX() + sliderW() + 4) {
                        dragging = (NumberSetting) setting;
                        dragging.setFraction((mouseX - sliderX()) / (float) sliderW());
                    }
                } else if (setting instanceof ModeSetting) {
                    // Left half or right-click goes back, right half goes forward.
                    boolean forward = mouseButton == 0 && mouseX >= (cx + right) / 2;
                    ((ModeSetting) setting).cycle(forward);
                    click();
                } else if (setting instanceof ColorSetting) {
                    ((ColorSetting) setting).cycle(mouseButton == 0);
                    click();
                } else if (setting instanceof KeySetting) {
                    listening = (KeySetting) setting;
                    click();
                }
            }
            XylarClient.get().markDirty();
            return;
        }
        super.mouseClicked(mouseX, mouseY, mouseButton);
    }

    @Override
    public void handleMouseInput() throws IOException {
        super.handleMouseInput();
        int wheel = Mouse.getEventDWheel();
        if (wheel != 0)
            scroll = Math.max(0, Math.min(maxScroll(), scroll - Integer.signum(wheel) * ROW));
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) throws IOException {
        if (listening != null) {
            if (keyCode == Keyboard.KEY_ESCAPE) {
                listening = null;
                return;
            }
            listening.set(keyCode == Keyboard.KEY_DELETE || keyCode == Keyboard.KEY_BACK ? 0 : keyCode);
            listening = null;
            XylarClient.get().markDirty();
            return;
        }
        if (keyCode == Keyboard.KEY_ESCAPE) {
            this.mc.displayGuiScreen(parent);
            return;
        }
        super.keyTyped(typedChar, keyCode);
    }

    @Override
    protected void actionPerformed(GuiButton button) throws IOException {
        if (button.id == BACK) {
            this.mc.displayGuiScreen(parent);
        } else if (button.id == RESET) {
            for (Setting<?> setting : module.settings())
                setting.reset();
            XylarClient.get().markDirty();
        }
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
