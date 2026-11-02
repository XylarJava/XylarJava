package io.xylarjava.client;

import org.lwjgl.opengl.GL11;

import io.xylarjava.client.util.CombatTracker;
import net.minecraft.block.Block;
import net.minecraft.block.material.Material;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.GuiNewChat;
import net.minecraft.client.network.NetworkPlayerInfo;
import net.minecraft.client.renderer.ActiveRenderInfo;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.renderer.RenderGlobal;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.entity.Entity;
import net.minecraft.entity.EntityLivingBase;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.potion.Potion;
import net.minecraft.util.BlockPos;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.IChatComponent;
import net.minecraft.util.MovingObjectPosition;
import net.minecraft.world.World;

/** What the mixins call. Each hook is a quick enabled check first, so a disabled module costs nothing. */
public final class Hooks {

    private static final int COMPACT_CHAT_IDS = 0x584A0000;

    private static boolean printingChat;
    private static String lastChat;
    private static int lastChatCount;
    private static int nextChatId = COMPACT_CHAT_IDS;

    private Hooks() {}

    // ---- fog --------------------------------------------------------------

    /** After vanilla set up the fog: push it out of sight, except underwater, in lava or when blind. */
    public static void afterSetupFog(int startCoords, float partialTicks) {
        if (!Modules.NO_FOG.isEnabled())
            return;
        Minecraft mc = Minecraft.getMinecraft();
        Entity view = mc.getRenderViewEntity();
        if (view == null || mc.theWorld == null)
            return;
        if (view instanceof EntityLivingBase && ((EntityLivingBase) view).isPotionActive(Potion.blindness))
            return;
        Material material = ActiveRenderInfo.getBlockAtEntityViewpoint(mc.theWorld, view, partialTicks).getMaterial();
        if (material == Material.water || material == Material.lava)
            return;
        GlStateManager.setFogStart(1000000f);
        GlStateManager.setFogEnd(1000001f);
    }

    // ---- block outline ------------------------------------------------------

    /** Draws the selection box in the Block Outline colours. Returns true when vanilla should be skipped. */
    public static boolean drawBlockOutline(World world, EntityPlayer player, MovingObjectPosition hit, int execute, float partialTicks) {
        if (!Modules.BLOCK_OUTLINE.isEnabled())
            return false;
        if (execute != 0 || hit == null || hit.typeOfHit != MovingObjectPosition.MovingObjectType.BLOCK || world == null)
            return true;
        BlockPos pos = hit.getBlockPos();
        Block block = world.getBlockState(pos).getBlock();
        if (block.getMaterial() == Material.air || !world.getWorldBorder().contains(pos))
            return true;
        int color = Modules.BLOCK_OUTLINE.color.argb();
        float alpha = Modules.BLOCK_OUTLINE.opacity.value() / 100f;
        GlStateManager.enableBlend();
        GlStateManager.tryBlendFuncSeparate(770, 771, 1, 0);
        GlStateManager.color(((color >> 16) & 255) / 255f, ((color >> 8) & 255) / 255f, (color & 255) / 255f, alpha);
        GL11.glLineWidth(Modules.BLOCK_OUTLINE.width.value());
        GlStateManager.disableTexture2D();
        GlStateManager.depthMask(false);
        block.setBlockBoundsBasedOnState(world, pos);
        double x = player.lastTickPosX + (player.posX - player.lastTickPosX) * partialTicks;
        double y = player.lastTickPosY + (player.posY - player.lastTickPosY) * partialTicks;
        double z = player.lastTickPosZ + (player.posZ - player.lastTickPosZ) * partialTicks;
        RenderGlobal.drawSelectionBoundingBox(block.getSelectedBoundingBox(world, pos).expand(0.002, 0.002, 0.002).offset(-x, -y, -z));
        GlStateManager.depthMask(true);
        GlStateManager.enableTexture2D();
        GlStateManager.disableBlend();
        GlStateManager.color(1f, 1f, 1f, 1f);
        return true;
    }

    // ---- chat -------------------------------------------------------------

    /** Timestamps, stacking of repeated lines and the mention sound. Returns true when the original call should be skipped. */
    public static boolean onChatMessage(GuiNewChat chat, IChatComponent message) {
        if (printingChat || message == null)
            return false;
        Minecraft mc = Minecraft.getMinecraft();
        String plain = message.getUnformattedText();

        if (Modules.MENTION_PING.isEnabled() && mc.thePlayer != null) {
            String name = mc.getSession().getUsername();
            if (name != null && !name.isEmpty() && plain.toLowerCase().contains(name.toLowerCase()) && !plain.startsWith("<" + name + ">"))
                Modules.MENTION_PING.ping();
        }

        boolean compact = Modules.COMPACT_CHAT.isEnabled();
        boolean stamps = Modules.TIMESTAMPS.isEnabled();
        if (!compact && !stamps)
            return false;

        IChatComponent out = message;
        int id = 0;
        if (compact) {
            if (plain.equals(lastChat) && !plain.trim().isEmpty()) {
                lastChatCount++;
                out = message.createCopy().appendSibling(new ChatComponentText(" §7(x" + lastChatCount + ")"));
            } else {
                lastChat = plain;
                lastChatCount = 1;
                nextChatId = nextChatId >= COMPACT_CHAT_IDS + 0xFFFF ? COMPACT_CHAT_IDS : nextChatId + 1;
            }
            id = nextChatId;
        }
        if (stamps)
            out = new ChatComponentText(Modules.TIMESTAMPS.stamp()).appendSibling(out);

        printingChat = true;
        try {
            chat.printChatMessageWithOptionalDeletion(out, id);
        } finally {
            printingChat = false;
        }
        return true;
    }

    /** Replaces your name in drawn text (Name Hider). */
    public static String hideName(String text) {
        if (text == null || !Modules.NAME_HIDER.isEnabled())
            return text;
        Minecraft mc = Minecraft.getMinecraft();
        String name = mc.getSession() == null ? null : mc.getSession().getUsername();
        if (name == null || name.isEmpty() || !text.contains(name))
            return text;
        return text.replace(name, Modules.NAME_HIDER.replacement());
    }

    // ---- combat -------------------------------------------------------------

    public static void onAttack(EntityPlayer player, Entity target) {
        if (player == null || target == null)
            return;
        CombatTracker.onAttack(player, target);
        if (Modules.HIT_PARTICLES.isEnabled() && player instanceof EntityPlayerSP)
            Modules.HIT_PARTICLES.onAttack((EntityPlayerSP) player, target);
    }

    // ---- tab list -----------------------------------------------------------

    /** Ping as a coloured number where the signal bars would be. */
    public static void drawNumericPing(int width, int x, int y, NetworkPlayerInfo info) {
        FontRenderer font = Minecraft.getMinecraft().fontRendererObj;
        int ping = info.getResponseTime();
        int color = ping < 0 ? 0xFF8E94A8 : ping < 80 ? 0xFF55FF55 : ping < 150 ? 0xFFFFFF55 : ping < 300 ? 0xFFFF9A3C : 0xFFFF5555;
        String text = ping < 0 ? "?" : String.valueOf(ping);
        GlStateManager.pushMatrix();
        GlStateManager.translate(0f, 0f, 100f);
        GlStateManager.scale(0.5f, 0.5f, 1f);
        font.drawStringWithShadow(text, (x + width - 1) * 2 - font.getStringWidth(text), y * 2 + 4, color);
        GlStateManager.popMatrix();
        GlStateManager.color(1f, 1f, 1f, 1f);
    }
}
