package io.xylarjava.client.gui;

import java.io.IOException;

import io.xylarjava.client.XylarClient;
import io.xylarjava.client.mixin.GuiMainMenuAccessor;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.gui.GuiButton;
import net.minecraft.client.gui.GuiMainMenu;
import net.minecraft.client.gui.GuiMultiplayer;
import net.minecraft.client.gui.GuiOptions;
import net.minecraft.client.gui.GuiSelectWorld;
import net.minecraft.client.renderer.GlStateManager;

/**
 * The XylarJava title screen. It extends the vanilla one only to reuse its rotating
 * panorama; the title, buttons and layout are our own. The big XYLARJAVA title is
 * drawn with the game's own font, scaled up.
 */
public class XylarMainMenu extends GuiMainMenu {

    private static final int SINGLEPLAYER = 1;
    private static final int MULTIPLAYER = 2;
    private static final int MODS = 10;
    private static final int SETTINGS = 0;
    private static final int QUIT = 4;

    private long openedAt;

    @Override
    public void initGui() {
        // Vanilla sets up the panorama textures here; we replace its buttons with ours.
        super.initGui();
        this.buttonList.clear();
        openedAt = System.currentTimeMillis();

        int buttonWidth = 200;
        int buttonHeight = 22;
        int x = this.width / 2 - buttonWidth / 2;
        int y = this.height / 2 - 18;
        int step = buttonHeight + 6;

        this.buttonList.add(new XButton(SINGLEPLAYER, x, y, buttonWidth, buttonHeight, "Singleplayer"));
        this.buttonList.add(new XButton(MULTIPLAYER, x, y + step, buttonWidth, buttonHeight, "Multiplayer"));
        this.buttonList.add(new XButton(MODS, x, y + step * 2, (buttonWidth - 6) / 2, buttonHeight, "Mods"));
        this.buttonList.add(new XButton(SETTINGS, x + (buttonWidth + 6) / 2, y + step * 2, (buttonWidth - 6) / 2, buttonHeight, "Settings"));
        this.buttonList.add(new XButton(QUIT, x, y + step * 3 + 10, buttonWidth, buttonHeight, "Quit Game", true));
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        GlStateManager.disableAlpha();
        ((GuiMainMenuAccessor) (Object) this).xylar$renderSkybox(mouseX, mouseY, partialTicks);
        GlStateManager.enableAlpha();

        // Darken the panorama so the menu reads well.
        this.drawGradientRect(0, 0, this.width, this.height, 0x99000000, 0xDD000000);

        // Title: slides in and fades up on open.
        float t = Math.min(1f, (System.currentTimeMillis() - openedAt) / 450f);
        float ease = 1f - (1f - t) * (1f - t);
        int alpha = Math.max(8, Math.round(255 * ease));
        int titleY = this.height / 2 - 78 - Math.round((1f - ease) * 12f);
        Draw.bigText("XYLARJAVA", this.width / 2, titleY, 4f, 1, (alpha << 24) | 0xFFFFFF);
        Draw.bigText("CLIENT  1.8.9", this.width / 2, titleY + 38, 1f, 2, (alpha << 24) | (Draw.ACCENT & 0xFFFFFF));

        for (GuiButton button : this.buttonList)
            button.drawButton(this.mc, mouseX, mouseY);

        String left = XylarClient.NAME + " Client " + XylarClient.get().version();
        this.fontRendererObj.drawStringWithShadow(left, 4, this.height - 12, Draw.MUTED);
        String right = "Minecraft 1.8.9";
        this.fontRendererObj.drawStringWithShadow(right, this.width - 4 - this.fontRendererObj.getStringWidth(right), this.height - 12, Draw.MUTED);
    }

    @Override
    protected void actionPerformed(GuiButton button) throws IOException {
        switch (button.id) {
            case SINGLEPLAYER:
                this.mc.displayGuiScreen(new GuiSelectWorld(this));
                break;
            case MULTIPLAYER:
                this.mc.displayGuiScreen(new GuiMultiplayer(this));
                break;
            case MODS:
                this.mc.displayGuiScreen(new ModMenuScreen(this));
                break;
            case SETTINGS:
                this.mc.displayGuiScreen(new GuiOptions(this, this.mc.gameSettings));
                break;
            case QUIT:
                this.mc.shutdown();
                break;
            default:
                break;
        }
    }
}
