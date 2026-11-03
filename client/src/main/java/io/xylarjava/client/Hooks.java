package io.xylarjava.client;

import org.lwjgl.opengl.GL11;

import io.xylarjava.client.util.CombatTracker;
import io.xylarjava.client.util.ShaderEffects;
import net.minecraft.block.Block;
import net.minecraft.block.material.Material;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.GuiNewChat;
import net.minecraft.client.network.NetworkPlayerInfo;
import net.minecraft.client.renderer.ActiveRenderInfo;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.renderer.EntityRenderer;
import net.minecraft.client.renderer.RenderGlobal;
import net.minecraft.client.renderer.block.model.ItemTransformVec3f;
import net.minecraft.client.resources.model.IBakedModel;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.entity.Entity;
import net.minecraft.entity.EntityLivingBase;
import net.minecraft.entity.item.EntityItem;
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
        boolean noFog = Modules.NO_FOG.isEnabled();
        boolean custom = Modules.FOG_CUSTOMIZER.isEnabled() && Modules.FOG_CUSTOMIZER.distance() != 1f;
        if ((!noFog && !custom) || !normalFog(partialTicks))
            return;
        if (noFog) {
            GlStateManager.setFogStart(1000000f);
            GlStateManager.setFogEnd(1000001f);
            return;
        }
        // Same shape as vanilla's overworld fog (sky starts at 0, terrain at 3/4), only further or closer.
        Minecraft mc = Minecraft.getMinecraft();
        if (mc.theWorld.provider.doesXZShowFog(0, 0))
            return;  // the nether's thick fog stays
        float far = mc.gameSettings.renderDistanceChunks * 16f * Modules.FOG_CUSTOMIZER.distance();
        GlStateManager.setFogStart(startCoords == -1 ? 0f : far * 0.75f);
        GlStateManager.setFogEnd(far);
    }

    /** Fog colour after vanilla picked it; null keeps it. */
    public static float[] fogColor(float red, float green, float blue, float partialTicks) {
        if (!Modules.FOG_CUSTOMIZER.isEnabled() || !normalFog(partialTicks))
            return null;
        float[] rgb = Modules.FOG_CUSTOMIZER.tint(red, green, blue);
        if (rgb != null)
            GlStateManager.clearColor(rgb[0], rgb[1], rgb[2], 0f);
        return rgb;
    }

    /** True outside water and lava, and when not blind: the fog the fog modules may change. */
    private static boolean normalFog(float partialTicks) {
        Minecraft mc = Minecraft.getMinecraft();
        Entity view = mc.getRenderViewEntity();
        if (view == null || mc.theWorld == null)
            return false;
        if (view instanceof EntityLivingBase && ((EntityLivingBase) view).isPotionActive(Potion.blindness))
            return false;
        Material material = ActiveRenderInfo.getBlockAtEntityViewpoint(mc.theWorld, view, partialTicks).getMaterial();
        return material != Material.water && material != Material.lava;
    }

    // ---- frame and world ----------------------------------------------------

    public static void beforeFrame(EntityRenderer renderer) {
        ShaderEffects.update(renderer);
    }

    /** Inside the world pass, before the hand. */
    public static void renderWorldOverlays(float partialTicks) {
        if (Modules.WAYPOINTS.isEnabled())
            Modules.WAYPOINTS.render(partialTicks);
    }

    // ---- dropped items --------------------------------------------------------

    /**
     * Item Physics: places a dropped item for drawing, lying flat (or upright for blocks) and turned
     * a fixed way, instead of bobbing and spinning. Returns how many copies to draw, like vanilla.
     */
    public static int placeDroppedItem(EntityItem item, double x, double y, double z, float partialTicks, IBakedModel model, int count) {
        boolean block = model.isGui3d();
        ItemTransformVec3f ground = model.getItemCameraTransforms().ground;
        float yaw = item.hoverStart * (180f / (float) Math.PI);
        boolean falling = !item.onGround && !item.isInWater() && Modules.ITEM_PHYSICS.tumble();
        float spin = falling ? (item.getAge() + partialTicks) * 18f + yaw : 0f;
        if (block) {
            GlStateManager.translate((float) x, (float) y + 0.25f * ground.scale.y, (float) z);
            GlStateManager.rotate(yaw, 0f, 1f, 0f);
            if (falling)
                GlStateManager.rotate(spin, 1f, 0f, 1f);
        } else {
            GlStateManager.translate((float) x, (float) y + 0.02f, (float) z);
            GlStateManager.rotate(yaw, 0f, 1f, 0f);
            GlStateManager.rotate(-90f + spin, 1f, 0f, 0f);
            // The ground transform lifts items by its translation; flat on the ground that would push them sideways.
            GlStateManager.translate(0f, -ground.translation.y, 0f);
        }
        GlStateManager.color(1f, 1f, 1f, 1f);
        return count;
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
        Modules.AUTO_GG.onChat(plain);

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
