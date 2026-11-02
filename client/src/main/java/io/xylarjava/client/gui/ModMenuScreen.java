package io.xylarjava.client.gui;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import org.lwjgl.input.Keyboard;
import org.lwjgl.input.Mouse;

import io.xylarjava.client.Modules;
import io.xylarjava.client.XylarClient;
import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.audio.PositionedSoundRecord;
import net.minecraft.client.gui.GuiButton;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.GuiTextField;
import net.minecraft.util.ResourceLocation;

/**
 * Right Shift menu: categories on the left, a search box, and a scrolling grid of mod cards.
 * Click a card to switch it on or off; click its settings icon (or right-click the card) for options.
 */
public class ModMenuScreen extends GuiScreen {

    private static final int EDIT_HUD = 1;
    private static final int FPS_PRESET = 2;
    private static final int DONE = 3;

    private static final int CARD_HEIGHT = 40;
    private static final int GAP = 5;
    private static final int SIDEBAR = 100;

    private static Category lastCategory;  // null = all
    private static String lastSearch = "";

    private final GuiScreen parent;
    private GuiTextField search;
    private Category category = lastCategory;
    private final List<Module> shown = new ArrayList<Module>();
    private int panelX, panelY, panelW, panelH;
    private int gridX, gridY, gridW, gridH, columns, cardW;
    private float scroll;
    private float scrollTarget;

    public ModMenuScreen(GuiScreen parent) {
        this.parent = parent;
    }

    @Override
    public void initGui() {
        Keyboard.enableRepeatEvents(true);
        panelW = Math.min(600, this.width - 20);
        panelH = Math.min(360, this.height - 20);
        panelX = (this.width - panelW) / 2;
        panelY = (this.height - panelH) / 2;

        gridX = panelX + SIDEBAR + 10;
        gridW = panelX + panelW - 10 - gridX;
        gridY = panelY + 52;
        gridH = panelY + panelH - 36 - gridY;
        columns = gridW >= 420 ? 3 : gridW >= 260 ? 2 : 1;
        cardW = (gridW - GAP * (columns - 1)) / columns;

        String text = search != null ? search.getText() : lastSearch;
        search = new GuiTextField(0, this.fontRendererObj, gridX + 1, panelY + 31, gridW - 2, 14);
        search.setMaxStringLength(40);
        search.setText(text);
        search.setFocused(true);
        search.setCanLoseFocus(false);
        search.setEnableBackgroundDrawing(false);

        this.buttonList.clear();
        int buttonY = panelY + panelH - 28;
        this.buttonList.add(new XButton(EDIT_HUD, gridX, buttonY, 100, 20, "Edit HUD layout"));
        this.buttonList.add(new XButton(FPS_PRESET, gridX + 106, buttonY, 100, 20, presetLabel()));
        this.buttonList.add(new XButton(DONE, panelX + panelW - 10 - 70, buttonY, 70, 20, "Done", true));
        refresh();
    }

    private static String presetLabel() {
        return Modules.OPTIMIZED_SETTINGS.isEnabled() ? "Undo FPS preset" : "FPS boost preset";
    }

    private void refresh() {
        shown.clear();
        String query = search == null ? "" : search.getText().trim().toLowerCase(Locale.ROOT);
        for (Module module : XylarClient.get().modules()) {
            if (category != null && module.category() != category)
                continue;
            if (!query.isEmpty() && !module.name().toLowerCase(Locale.ROOT).contains(query)
                    && !module.description().toLowerCase(Locale.ROOT).contains(query))
                continue;
            shown.add(module);
        }
        scrollTarget = Math.min(scrollTarget, maxScroll());
    }

    private int contentHeight() {
        int rows = (shown.size() + columns - 1) / columns;
        return rows * (CARD_HEIGHT + GAP) - GAP;
    }

    private float maxScroll() {
        return Math.max(0, contentHeight() - gridH);
    }

    @Override
    public void updateScreen() {
        search.updateCursorCounter();
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        if (this.mc.theWorld != null)
            this.drawGradientRect(0, 0, this.width, this.height, 0x99000000, 0xBB000000);
        else
            this.drawDefaultBackground();
        scroll += (scrollTarget - scroll) * 0.5f;

        // Panel and header
        Draw.softRect(panelX, panelY, panelW, panelH, 0xF20E1016);
        Draw.outline(panelX, panelY, panelW, panelH, Draw.BORDER);
        this.fontRendererObj.drawStringWithShadow("XYLARJAVA", panelX + 10, panelY + 10, Draw.ACCENT);
        this.fontRendererObj.drawStringWithShadow("Mods", panelX + 10 + this.fontRendererObj.getStringWidth("XYLARJAVA "), panelY + 10, Draw.TEXT);
        int on = 0;
        for (Module module : XylarClient.get().modules()) {
            if (module.isEnabled())
                on++;
        }
        String count = XylarClient.get().modules().size() + " mods  -  " + on + " on";
        this.fontRendererObj.drawString(count, panelX + panelW - 10 - this.fontRendererObj.getStringWidth(count), panelY + 10, Draw.MUTED);

        // Categories
        int tabY = panelY + 30;
        drawTab("All", null, tabY, mouseX, mouseY);
        for (Category c : Category.values()) {
            tabY += 20;
            drawTab(c.title(), c, tabY, mouseX, mouseY);
        }
        Draw.rect(panelX + SIDEBAR + 4, panelY + 28, 1, panelH - 38, 0x22FFFFFF);

        // Search
        Draw.softRect(gridX - 2, panelY + 27, gridW + 4, 18, 0xFF161922);
        Draw.outline(gridX - 2, panelY + 27, gridW + 4, 18, search.isFocused() ? 0x88FF9A3C : 0x22FFFFFF);
        if (search.getText().isEmpty())
            this.fontRendererObj.drawString("Search " + (category == null ? "all mods" : category.title()) + "...",
                    gridX + 2, panelY + 32, Draw.MUTED);
        search.xPosition = gridX + 2;
        search.yPosition = panelY + 32;
        search.drawTextBox();

        // Cards (clipped to the grid)
        Draw.scissor(this.mc, gridX - 1, gridY, gridW + 2, gridH);
        int offset = Math.round(scroll);
        for (int i = 0; i < shown.size(); i++) {
            int x = cardX(i);
            int y = cardY(i) - offset;
            if (y + CARD_HEIGHT < gridY || y > gridY + gridH)
                continue;
            boolean hovered = insideGrid(mouseX, mouseY) && Draw.inside(mouseX, mouseY, x, y, cardW, CARD_HEIGHT);
            drawCard(shown.get(i), x, y, hovered, mouseX, mouseY);
        }
        Draw.endScissor();
        if (shown.isEmpty())
            Draw.centeredText("Nothing matches \"" + search.getText() + "\"", gridX + gridW / 2, gridY + 30, Draw.MUTED);

        // Scrollbar
        if (maxScroll() > 0) {
            int barH = Math.max(20, gridH * gridH / contentHeight());
            int barY = gridY + Math.round((gridH - barH) * (scroll / maxScroll()));
            Draw.rect(panelX + panelW - 6, gridY, 2, gridH, 0x22FFFFFF);
            Draw.rect(panelX + panelW - 6, barY, 2, barH, Draw.ACCENT);
        }

        super.drawScreen(mouseX, mouseY, partialTicks);
    }

    private void drawTab(String title, Category c, int y, int mouseX, int mouseY) {
        boolean active = category == c;
        boolean hovered = Draw.inside(mouseX, mouseY, panelX + 6, y, SIDEBAR - 6, 17);
        if (active || hovered)
            Draw.softRect(panelX + 6, y, SIDEBAR - 6, 17, active ? 0xFF232736 : 0xFF181B24);
        if (active)
            Draw.rect(panelX + 6, y + 3, 2, 11, Draw.ACCENT);
        this.fontRendererObj.drawString(title, panelX + 14, y + 5, active ? Draw.TEXT : Draw.MUTED);
        int n = 0;
        for (Module module : XylarClient.get().modules()) {
            if (c == null || module.category() == c)
                n++;
        }
        String count = String.valueOf(n);
        this.fontRendererObj.drawString(count, panelX + SIDEBAR - 4 - this.fontRendererObj.getStringWidth(count), y + 5, 0xFF5A6075);
    }

    private void drawCard(Module module, int x, int y, boolean hovered, int mouseX, int mouseY) {
        boolean on = module.isEnabled();
        Draw.softRect(x, y, cardW, CARD_HEIGHT, hovered ? Draw.PANEL_HOVER : 0xFF181B24);
        if (on)
            Draw.rect(x, y + 6, 2, CARD_HEIGHT - 12, Draw.ACCENT);

        int nameWidth = cardW - (module.settings().isEmpty() ? 42 : 56);
        String name = ellipsize(module.name(), nameWidth);
        this.fontRendererObj.drawStringWithShadow(name, x + 9, y + 7, on ? Draw.TEXT : 0xFFC4C8D4);
        this.fontRendererObj.drawString(ellipsize(module.description(), cardW - 18), x + 9, y + 22, Draw.MUTED);

        // Switch
        int sx = x + cardW - 31;
        int sy = y + 6;
        Draw.softRect(sx, sy, 24, 11, on ? Draw.ACCENT : 0xFF2C3142);
        Draw.rect(on ? sx + 14 : sx + 2, sy + 2, 8, 7, on ? 0xFF1A1006 : Draw.MUTED);

        // Settings icon (three slider lines) when the module has options
        if (!module.settings().isEmpty()) {
            int ix = x + cardW - 44;
            int iy = y + 7;
            boolean iconHover = Draw.inside(mouseX, mouseY, ix - 2, iy - 2, 12, 12);
            int c = iconHover ? Draw.ACCENT : 0xFF6A7088;
            Draw.rect(ix, iy + 1, 8, 1, c);
            Draw.rect(ix, iy + 4, 8, 1, c);
            Draw.rect(ix, iy + 7, 8, 1, c);
            Draw.rect(ix + 5, iy, 2, 3, c);
            Draw.rect(ix + 1, iy + 3, 2, 3, c);
            Draw.rect(ix + 4, iy + 6, 2, 3, c);
        }
    }

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
        return gridX + (index % columns) * (cardW + GAP);
    }

    private int cardY(int index) {
        return gridY + (index / columns) * (CARD_HEIGHT + GAP);
    }

    private boolean insideGrid(int mouseX, int mouseY) {
        return Draw.inside(mouseX, mouseY, gridX, gridY, gridW, gridH);
    }

    private void click() {
        this.mc.getSoundHandler().playSound(PositionedSoundRecord.create(new ResourceLocation("gui.button.press"), 1.0F));
    }

    @Override
    protected void mouseClicked(int mouseX, int mouseY, int mouseButton) throws IOException {
        // Category tabs
        int tabY = panelY + 30;
        for (int i = -1; i < Category.values().length; i++) {
            if (Draw.inside(mouseX, mouseY, panelX + 6, tabY, SIDEBAR - 6, 17)) {
                category = i < 0 ? null : Category.values()[i];
                lastCategory = category;
                scrollTarget = scroll = 0;
                refresh();
                click();
                return;
            }
            tabY += 20;
        }
        search.mouseClicked(mouseX, mouseY, mouseButton);

        if (insideGrid(mouseX, mouseY)) {
            int offset = Math.round(scroll);
            for (int i = 0; i < shown.size(); i++) {
                int x = cardX(i);
                int y = cardY(i) - offset;
                if (!Draw.inside(mouseX, mouseY, x, y, cardW, CARD_HEIGHT))
                    continue;
                Module module = shown.get(i);
                boolean icon = !module.settings().isEmpty() && Draw.inside(mouseX, mouseY, x + cardW - 46, y + 5, 12, 12);
                if (!module.settings().isEmpty() && (icon || mouseButton == 1)) {
                    click();
                    this.mc.displayGuiScreen(new ModuleSettingsScreen(this, module));
                    return;
                }
                if (mouseButton == 0) {
                    module.toggle();
                    XylarClient.get().markDirty();
                    click();
                }
                return;
            }
        }
        super.mouseClicked(mouseX, mouseY, mouseButton);
    }

    @Override
    public void handleMouseInput() throws IOException {
        super.handleMouseInput();
        int wheel = Mouse.getEventDWheel();
        if (wheel != 0) {
            scrollTarget = Math.max(0, Math.min(maxScroll(), scrollTarget - Integer.signum(wheel) * (CARD_HEIGHT + GAP)));
        }
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) throws IOException {
        if (keyCode == Keyboard.KEY_ESCAPE) {
            this.mc.displayGuiScreen(parent);
            return;
        }
        if (search.textboxKeyTyped(typedChar, keyCode)) {
            lastSearch = search.getText();
            scrollTarget = scroll = 0;
            refresh();
            return;
        }
        super.keyTyped(typedChar, keyCode);
    }

    @Override
    protected void actionPerformed(GuiButton button) throws IOException {
        if (button.id == EDIT_HUD) {
            this.mc.displayGuiScreen(new HudEditorScreen(this));
        } else if (button.id == FPS_PRESET) {
            Modules.OPTIMIZED_SETTINGS.toggle();
            XylarClient.get().markDirty();
            button.displayString = presetLabel();
        } else if (button.id == DONE) {
            this.mc.displayGuiScreen(parent);
        }
    }

    @Override
    public void onGuiClosed() {
        Keyboard.enableRepeatEvents(false);
        XylarClient.get().saveNow();
    }

    @Override
    public boolean doesGuiPauseGame() {
        return false;
    }
}
