package io.xylarjava.client.module.impl;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import org.lwjgl.opengl.GL11;

import io.xylarjava.client.XylarClient;
import io.xylarjava.client.config.ClientConfig;
import io.xylarjava.client.gui.WaypointsScreen;
import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ColorSetting;
import io.xylarjava.client.setting.KeySetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.multiplayer.ServerData;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.renderer.Tessellator;
import net.minecraft.client.renderer.WorldRenderer;
import net.minecraft.client.renderer.entity.RenderManager;
import net.minecraft.client.renderer.vertex.DefaultVertexFormats;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.MathHelper;

/**
 * Marked places, kept per server (or singleplayer world) and dimension, drawn as a beam and a
 * label you can see through walls. B marks where you stand, N opens the list.
 */
public class WaypointsModule extends Module {

    public static class Waypoint {
        public String name;
        public final String world;
        public final int dimension;
        public final int x, y, z;
        public int color;
        public boolean visible = true;

        Waypoint(String name, String world, int dimension, int x, int y, int z, int color) {
            this.name = name;
            this.world = world;
            this.dimension = dimension;
            this.x = x;
            this.y = y;
            this.z = z;
            this.color = color;
        }

        String save() {
            return world + "|" + dimension + "|" + x + "|" + y + "|" + z + "|" + String.format("%08X", color) + "|" + visible + "|" + name;
        }

        static Waypoint load(String text) {
            String[] parts = text.split("\\|", 8);
            if (parts.length < 8)
                return null;
            try {
                Waypoint wp = new Waypoint(parts[7], parts[0], Integer.parseInt(parts[1]), Integer.parseInt(parts[2]), Integer.parseInt(parts[3]),
                        Integer.parseInt(parts[4]), (int) Long.parseLong(parts[5], 16));
                wp.visible = Boolean.parseBoolean(parts[6]);
                return wp;
            } catch (NumberFormatException e) {
                return null;
            }
        }

        public double distanceTo(EntityPlayerSP player) {
            double dx = x + 0.5 - player.posX, dy = y - player.posY, dz = z + 0.5 - player.posZ;
            return Math.sqrt(dx * dx + dy * dy + dz * dz);
        }
    }

    private static final String KEY_PREFIX = "wp.";

    private final KeySetting addKey = key("addkey", "Add waypoint key", 48);  // B
    private final KeySetting menuKey = key("menukey", "Waypoint list key", 49);  // N
    private final BoolSetting deaths = bool("deaths", "Mark where you die", true);
    private final BoolSetting beam = bool("beam", "Beam", true);
    private final NumberSetting labelSize = number("labelsize", "Label size", 100f, 50f, 200f, 10f, "%");
    private final ColorSetting color = color("color", "Colour for new ones", 0xFF3CC8FF);

    private final List<Waypoint> waypoints = new ArrayList<Waypoint>();
    private boolean addWasDown;
    private boolean menuWasDown;
    private boolean wasDead;

    public WaypointsModule() {
        super("waypoints", "Waypoints", "Mark places with B, see them through walls, list them with N.", Category.UTILITY);
    }

    @Override
    protected boolean enabledByDefault() {
        return true;
    }

    @Override
    public void attach(ClientConfig config) {
        super.attach(config);
        waypoints.clear();
        for (Map.Entry<String, String> entry : state.settings.entrySet()) {
            if (entry.getKey().startsWith(KEY_PREFIX)) {
                Waypoint wp = Waypoint.load(entry.getValue());
                if (wp != null)
                    waypoints.add(wp);
            }
        }
    }

    @Override
    public void storeSettings() {
        super.storeSettings();
        if (state == null)
            return;
        Iterator<String> keys = state.settings.keySet().iterator();
        while (keys.hasNext()) {
            if (keys.next().startsWith(KEY_PREFIX))
                keys.remove();
        }
        for (int i = 0; i < waypoints.size(); i++)
            state.settings.put(KEY_PREFIX + i, waypoints.get(i).save());
    }

    /** Server address or singleplayer world, so waypoints stay where they were made. */
    public static String currentWorld() {
        Minecraft mc = Minecraft.getMinecraft();
        if (mc.isSingleplayer() && mc.getIntegratedServer() != null)
            return "sp:" + mc.getIntegratedServer().getFolderName();
        ServerData data = mc.getCurrentServerData();
        return data == null ? "unknown" : "mp:" + data.serverIP.toLowerCase(Locale.ROOT);
    }

    /** Waypoints of the world and dimension you're in. */
    public List<Waypoint> here() {
        List<Waypoint> out = new ArrayList<Waypoint>();
        EntityPlayerSP player = mc().thePlayer;
        if (player == null)
            return out;
        String world = currentWorld();
        for (Waypoint wp : waypoints) {
            if (wp.world.equals(world) && wp.dimension == player.dimension)
                out.add(wp);
        }
        return out;
    }

    public Waypoint addHere(String name) {
        EntityPlayerSP player = mc().thePlayer;
        if (player == null)
            return null;
        if (name == null || name.trim().isEmpty())
            name = "Waypoint " + (here().size() + 1);
        Waypoint wp = new Waypoint(name.trim().replace('|', '/'), currentWorld(), player.dimension, MathHelper.floor_double(player.posX),
                MathHelper.floor_double(player.posY), MathHelper.floor_double(player.posZ), color.argb());
        waypoints.add(wp);
        XylarClient.get().markDirty();
        return wp;
    }

    public void remove(Waypoint wp) {
        waypoints.remove(wp);
        XylarClient.get().markDirty();
    }

    @Override
    public void onTick(Minecraft mc) {
        EntityPlayerSP player = mc.thePlayer;
        boolean playing = mc.currentScreen == null && player != null;
        boolean addDown = playing && addKey.isDown();
        boolean menuDown = playing && menuKey.isDown();
        if (addDown && !addWasDown) {
            Waypoint wp = addHere(null);
            if (wp != null)
                player.addChatMessage(new ChatComponentText("§6[XylarJava] §7Waypoint §f" + wp.name + " §7at §f" + wp.x + " " + wp.y + " " + wp.z
                        + "§7. Press §fN §7to see the list."));
        }
        if (menuDown && !menuWasDown)
            mc.displayGuiScreen(new WaypointsScreen());
        addWasDown = addDown;
        menuWasDown = menuDown;

        boolean dead = player != null && (player.isDead || player.getHealth() <= 0f);
        if (dead && !wasDead && deaths.on()) {
            for (Waypoint wp : here()) {
                if (wp.name.equals("Death"))
                    waypoints.remove(wp);
            }
            Waypoint wp = addHere("Death");
            if (wp != null)
                wp.color = 0xFFFF4F5E;
        }
        wasDead = dead;
    }

    // ---- drawing ------------------------------------------------------------

    /** In the world pass, before the hand: positions are relative to the camera's entity. */
    public void render(float partialTicks) {
        Minecraft mc = mc();
        if (mc.thePlayer == null || mc.gameSettings.hideGUI)
            return;
        List<Waypoint> shown = here();
        if (shown.isEmpty())
            return;
        RenderManager rm = mc.getRenderManager();
        double far = Math.max(16, mc.gameSettings.renderDistanceChunks * 16 - 8);

        GlStateManager.pushMatrix();
        GlStateManager.disableLighting();
        GlStateManager.disableDepth();
        GlStateManager.depthMask(false);
        GlStateManager.enableBlend();
        GlStateManager.tryBlendFuncSeparate(770, 771, 1, 0);
        GlStateManager.disableCull();
        for (Waypoint wp : shown) {
            if (!wp.visible)
                continue;
            double dx = wp.x + 0.5 - rm.viewerPosX;
            double dy = wp.y - rm.viewerPosY;
            double dz = wp.z + 0.5 - rm.viewerPosZ;
            double distance = Math.sqrt(dx * dx + dy * dy + dz * dz);
            // Past the far plane, pull it in along the same direction so it still shows.
            double k = distance > far ? far / distance : 1;
            if (beam.on())
                drawBeam(dx * k, dz * k, wp.color);
            drawLabel(wp, dx * k, dy * k + 1.2 * k, dz * k, distance * k, distance, rm);
        }
        GlStateManager.enableCull();
        GlStateManager.depthMask(true);
        GlStateManager.enableDepth();
        GlStateManager.enableTexture2D();
        GlStateManager.disableBlend();
        GlStateManager.color(1f, 1f, 1f, 1f);
        GlStateManager.popMatrix();
    }

    private void drawBeam(double x, double z, int color) {
        Tessellator tessellator = Tessellator.getInstance();
        WorldRenderer buffer = tessellator.getWorldRenderer();
        GlStateManager.disableTexture2D();
        float r = ((color >> 16) & 255) / 255f, g = ((color >> 8) & 255) / 255f, b = (color & 255) / 255f;
        double w = 0.12;
        double bottom = -mc().getRenderManager().viewerPosY;
        double top = bottom + 256;
        buffer.begin(GL11.GL_QUADS, DefaultVertexFormats.POSITION_COLOR);
        // Two crossed planes, so the beam has width from every side.
        buffer.pos(x - w, bottom, z).color(r, g, b, 0.35f).endVertex();
        buffer.pos(x + w, bottom, z).color(r, g, b, 0.35f).endVertex();
        buffer.pos(x + w, top, z).color(r, g, b, 0.0f).endVertex();
        buffer.pos(x - w, top, z).color(r, g, b, 0.0f).endVertex();
        buffer.pos(x, bottom, z - w).color(r, g, b, 0.35f).endVertex();
        buffer.pos(x, bottom, z + w).color(r, g, b, 0.35f).endVertex();
        buffer.pos(x, top, z + w).color(r, g, b, 0.0f).endVertex();
        buffer.pos(x, top, z - w).color(r, g, b, 0.0f).endVertex();
        tessellator.draw();
        GlStateManager.enableTexture2D();
    }

    private void drawLabel(Waypoint wp, double x, double y, double z, double drawDistance, double realDistance, RenderManager rm) {
        FontRenderer font = mc().fontRendererObj;
        String text = wp.name + " §7" + Math.round(realDistance) + "m";
        // Grows with distance so it keeps about the same size on screen.
        float scale = (float) (Math.max(4.0, drawDistance) * 0.0045 * labelSize.value() / 100f);
        GlStateManager.pushMatrix();
        GlStateManager.translate(x, y, z);
        GlStateManager.rotate(-rm.playerViewY, 0f, 1f, 0f);
        GlStateManager.rotate(rm.playerViewX * (mc().gameSettings.thirdPersonView == 2 ? -1 : 1), 1f, 0f, 0f);
        GlStateManager.scale(-scale, -scale, scale);
        int half = font.getStringWidth(text) / 2;
        Tessellator tessellator = Tessellator.getInstance();
        WorldRenderer buffer = tessellator.getWorldRenderer();
        GlStateManager.disableTexture2D();
        buffer.begin(GL11.GL_QUADS, DefaultVertexFormats.POSITION_COLOR);
        buffer.pos(-half - 3, -3, 0).color(0f, 0f, 0f, 0.45f).endVertex();
        buffer.pos(-half - 3, 10, 0).color(0f, 0f, 0f, 0.45f).endVertex();
        buffer.pos(half + 3, 10, 0).color(0f, 0f, 0f, 0.45f).endVertex();
        buffer.pos(half + 3, -3, 0).color(0f, 0f, 0f, 0.45f).endVertex();
        int c = wp.color;
        buffer.pos(-half - 3, 10, 0).color(((c >> 16) & 255) / 255f, ((c >> 8) & 255) / 255f, (c & 255) / 255f, 1f).endVertex();
        buffer.pos(-half - 3, 11, 0).color(((c >> 16) & 255) / 255f, ((c >> 8) & 255) / 255f, (c & 255) / 255f, 1f).endVertex();
        buffer.pos(half + 3, 11, 0).color(((c >> 16) & 255) / 255f, ((c >> 8) & 255) / 255f, (c & 255) / 255f, 1f).endVertex();
        buffer.pos(half + 3, 10, 0).color(((c >> 16) & 255) / 255f, ((c >> 8) & 255) / 255f, (c & 255) / 255f, 1f).endVertex();
        tessellator.draw();
        GlStateManager.enableTexture2D();
        font.drawString(text, -half, 0, 0xFFFFFFFF);
        GlStateManager.popMatrix();
    }
}
