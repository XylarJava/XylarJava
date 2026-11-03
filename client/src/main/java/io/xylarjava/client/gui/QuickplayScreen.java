package io.xylarjava.client.gui;

import io.xylarjava.client.module.impl.ServerModules;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.gui.GuiButton;
import net.minecraft.client.gui.GuiScreen;

/** Pick a Hypixel game and mode; one click joins it. */
public class QuickplayScreen extends GuiScreen {

    private static final String[] GAMES = {"Bed Wars", "SkyWars", "Duels", "Arcade", "More"};

    // For each game: mode name, command. Commands are Hypixel's own /play names.
    private static final String[][][] MODES = {
        {
            {"Solo", "/play bedwars_eight_one"}, {"Doubles", "/play bedwars_eight_two"}, {"3v3v3v3", "/play bedwars_four_three"},
            {"4v4v4v4", "/play bedwars_four_four"}, {"4v4", "/play bedwars_two_four"}, {"Lobby", "/lobby bedwars"},
        },
        {
            {"Solo Normal", "/play solo_normal"}, {"Solo Insane", "/play solo_insane"}, {"Teams Normal", "/play teams_normal"},
            {"Teams Insane", "/play teams_insane"}, {"Mini", "/play mini_normal"}, {"Lobby", "/lobby skywars"},
        },
        {
            {"Classic", "/play duels_classic_duel"}, {"Sumo", "/play duels_sumo_duel"}, {"Bridge", "/play duels_bridge_duel"},
            {"UHC", "/play duels_uhc_duel"}, {"OP", "/play duels_op_duel"}, {"Bow", "/play duels_bow_duel"},
            {"SkyWars", "/play duels_sw_duel"}, {"Combo", "/play duels_combo_duel"}, {"No Debuff", "/play duels_potion_duel"},
            {"Boxing", "/play duels_boxing_duel"}, {"Parkour", "/play duels_parkour_eight"}, {"Lobby", "/lobby duels"},
        },
        {
            {"Party Games", "/play arcade_party_games_1"}, {"Hole in the Wall", "/play arcade_hole_in_the_wall"},
            {"Mini Walls", "/play arcade_mini_walls"}, {"Football", "/play arcade_soccer"}, {"Zombies", "/play arcade_zombies_dead_end"},
            {"Lobby", "/lobby arcade"},
        },
        {
            {"Murder Mystery", "/play murder_classic"}, {"Build Battle", "/play build_battle_solo_normal"}, {"TNT Run", "/play tnt_tntrun"},
            {"Bow Spleef", "/play tnt_bowspleef"}, {"The Pit", "/play pit"}, {"UHC", "/play uhc_solo"},
            {"Mega Walls", "/play mw_standard"}, {"Blitz", "/play blitz_solo_normal"}, {"Main lobby", "/lobby"},
        },
    };

    private static int lastGame;

    private int game = lastGame;
    private int panelX, panelY, panelW, panelH;
    private boolean hypixel;

    @Override
    public void initGui() {
        hypixel = ServerModules.onHypixel();
        panelW = Math.min(380, this.width - 20);
        panelH = Math.min(220, this.height - 20);
        panelX = (this.width - panelW) / 2;
        panelY = (this.height - panelH) / 2;
        this.buttonList.clear();

        int tabW = (panelW - 20 - (GAMES.length - 1) * 4) / GAMES.length;
        for (int i = 0; i < GAMES.length; i++)
            this.buttonList.add(new XButton(100 + i, panelX + 10 + i * (tabW + 4), panelY + 30, tabW, 18, GAMES[i], i == game));

        String[][] modes = MODES[game];
        int columns = 3;
        int w = (panelW - 20 - (columns - 1) * 6) / columns;
        for (int i = 0; i < modes.length; i++) {
            int x = panelX + 10 + (i % columns) * (w + 6);
            int y = panelY + 60 + (i / columns) * 26;
            XButton button = new XButton(200 + i, x, y, w, 20, modes[i][0]);
            button.enabled = hypixel;
            this.buttonList.add(button);
        }
        this.buttonList.add(new XButton(1, panelX + panelW - 80, panelY + panelH - 28, 70, 20, "Close", true));
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        if (this.mc.theWorld != null)
            this.drawGradientRect(0, 0, this.width, this.height, 0x66000000, 0x99000000);
        else
            this.drawDefaultBackground();
        Draw.softRect(panelX, panelY, panelW, panelH, 0xF20E1016);
        Draw.outline(panelX, panelY, panelW, panelH, Draw.BORDER);
        this.fontRendererObj.drawStringWithShadow("Quickplay", panelX + 12, panelY + 12, Draw.TEXT);
        this.fontRendererObj.drawString("Hypixel", panelX + 12 + this.fontRendererObj.getStringWidth("Quickplay") + 6, panelY + 12, Draw.ACCENT);
        if (!hypixel)
            this.fontRendererObj.drawString("Join Hypixel to use these.", panelX + 12, panelY + panelH - 22, Draw.MUTED);
        super.drawScreen(mouseX, mouseY, partialTicks);
    }

    @Override
    protected void actionPerformed(GuiButton button) {
        if (button.id == 1) {
            this.mc.displayGuiScreen(null);
        } else if (button.id >= 200) {
            String command = MODES[game][button.id - 200][1];
            this.mc.displayGuiScreen(null);
            if (this.mc.thePlayer != null)
                this.mc.thePlayer.sendChatMessage(command);
        } else if (button.id >= 100) {
            game = lastGame = button.id - 100;
            initGui();
        }
    }

    @Override
    public boolean doesGuiPauseGame() {
        return false;
    }
}
