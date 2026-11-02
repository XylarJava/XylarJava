package io.xylarjava.client.gui;

import java.io.IOException;
import java.util.List;

import io.xylarjava.client.XylarClient;
import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.gui.GuiButton;
import net.minecraft.client.gui.GuiScreen;

/** Right Shift menu: switch modules on and off, and open the HUD editor. */
public class ModMenuScreen extends GuiScreen {

    private static final int EDIT_HUD = 1;
    private static final int DONE = 2;

    private static final int CARD_HEIGHT = 44;
    private static final int GAP = 6;

    private final GuiScreen parent;
    private int panelX;
    private int panelY;
    private int panelWidth;
    private int panelHeight;
    private int cardWidth;

    public ModMenuScreen(GuiScreen parent) {
        this.parent = parent;
    }

    @Override
    public void initGui() {
        List<Module> modules = XylarClient.get().modules();
        int rows = (modules.size() + 1) / 2;

        panelWidth = Math.min(440, this.width - 40);
        cardWidth = (panelWidth - 24 - GAP) / 2;
        panelHeight = Math.min(this.height - 30, 44 + rows * (CARD_HEIGHT + GAP) + 40);
        panelX = (this.width - panelWidth) / 2;
        panelY = (this.height - panelHeight) / 2;

        this.buttonList.clear();
        int buttonY = panelY + panelHeight - 30;
        this.buttonList.add(new XButton(EDIT_HUD, panelX + 12, buttonY, 120, 20, "Edit HUD layout"));
        this.buttonList.add(new XButton(DONE, panelX + panelWidth - 12 - 80, buttonY, 80, 20, "Done", true));
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        if (this.mc.theWorld != null)
            this.drawGradientRect(0, 0, this.width, this.height, 0x88000000, 0xAA000000);
        else
            this.drawDefaultBackground();

        Draw.softRect(panelX, panelY, panelWidth, panelHeight, 0xF0101218);
        Draw.outline(panelX, panelY, panelWidth, panelHeight, Draw.BORDER);
        this.fontRendererObj.drawStringWithShadow("XYLARJAVA", panelX + 12, panelY + 12, Draw.ACCENT);
        this.fontRendererObj.drawStringWithShadow("Mods", panelX + 12 + this.fontRendererObj.getStringWidth("XYLARJAVA "), panelY + 12, Draw.TEXT);
        String hint = "Click a card to turn it on or off";
        this.fontRendererObj.drawString(hint, panelX + panelWidth - 12 - this.fontRendererObj.getStringWidth(hint), panelY + 12, Draw.MUTED);

        List<Module> modules = XylarClient.get().modules();
        for (int i = 0; i < modules.size(); i++) {
            int x = cardX(i);
            int y = cardY(i);
            if (y + CARD_HEIGHT > panelY + panelHeight - 36)
                break;
            drawCard(modules.get(i), x, y, Draw.inside(mouseX, mouseY, x, y, cardWidth, CARD_HEIGHT));
        }

        super.drawScreen(mouseX, mouseY, partialTicks);
    }

    private void drawCard(Module module, int x, int y, boolean hovered) {
        boolean on = module.isEnabled();
        Draw.softRect(x, y, cardWidth, CARD_HEIGHT, hovered ? Draw.PANEL_HOVER : 0xFF181B24);
        if (on)
            Draw.rect(x, y + 6, 2, CARD_HEIGHT - 12, Draw.ACCENT);

        this.fontRendererObj.drawStringWithShadow(module.name(), x + 10, y + 8, Draw.TEXT);
        String kind = module instanceof HudModule ? "HUD" : "Gameplay";
        this.fontRendererObj.drawString(kind, x + 10 + this.fontRendererObj.getStringWidth(module.name()) + 6, y + 8, Draw.MUTED);
        // The switch sits on the title line, so the description can use the whole card width.
        String description = ellipsize(module.description(), cardWidth - 20);
        this.fontRendererObj.drawString(description, x + 10, y + 24, Draw.MUTED);

        // Switch
        int switchX = x + cardWidth - 36;
        int switchY = y + 8;
        Draw.softRect(switchX, switchY, 26, 12, on ? Draw.ACCENT : 0xFF2C3142);
        Draw.rect(on ? switchX + 15 : switchX + 2, switchY + 2, 9, 8, on ? 0xFF1A1006 : Draw.MUTED);
    }

    /** Shortens text to fit, cutting at a word and ending with "...". */
    private String ellipsize(String text, int width) {
        if (this.fontRendererObj.getStringWidth(text) <= width)
            return text;
        String cut = this.fontRendererObj.trimStringToWidth(text, width - this.fontRendererObj.getStringWidth("..."));
        int space = cut.lastIndexOf(' ');
        if (space > cut.length() / 2)
            cut = cut.substring(0, space);
        return cut.trim() + "...";
    }

    private int cardX(int index) {
        return panelX + 12 + (index % 2) * (cardWidth + GAP);
    }

    private int cardY(int index) {
        return panelY + 32 + (index / 2) * (CARD_HEIGHT + GAP);
    }

    @Override
    protected void mouseClicked(int mouseX, int mouseY, int mouseButton) throws IOException {
        if (mouseButton == 0) {
            List<Module> modules = XylarClient.get().modules();
            for (int i = 0; i < modules.size(); i++) {
                if (Draw.inside(mouseX, mouseY, cardX(i), cardY(i), cardWidth, CARD_HEIGHT)) {
                    modules.get(i).toggle();
                    XylarClient.get().markDirty();
                    this.mc.getSoundHandler().playSound(
                        net.minecraft.client.audio.PositionedSoundRecord.create(new net.minecraft.util.ResourceLocation("gui.button.press"), 1.0F));
                    return;
                }
            }
        }
        super.mouseClicked(mouseX, mouseY, mouseButton);
    }

    @Override
    protected void actionPerformed(GuiButton button) throws IOException {
        if (button.id == EDIT_HUD)
            this.mc.displayGuiScreen(new HudEditorScreen(this));
        else if (button.id == DONE)
            this.mc.displayGuiScreen(parent);
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
