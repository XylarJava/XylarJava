package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.util.EnumFacing;
import net.minecraft.util.MathHelper;

public class CoordinatesModule extends HudModule {

    public CoordinatesModule() {
        super("coordinates", "Coordinates", "Your position and the direction you're facing.", 0.01f, 0.11f);
    }

    @Override
    public int width() {
        return 100;
    }

    @Override
    public int height() {
        return 40;
    }

    @Override
    protected void draw(boolean editing) {
        Draw.rect(0, 0, width(), height(), Draw.PANEL);
        EntityPlayerSP player = mc().thePlayer;
        String x = "X: -", y = "Y: -", z = "Z: -", facing = "";
        if (player != null) {
            x = "X: " + MathHelper.floor_double(player.posX);
            y = "Y: " + MathHelper.floor_double(player.getEntityBoundingBox().minY);
            z = "Z: " + MathHelper.floor_double(player.posZ);
            EnumFacing direction = player.getHorizontalFacing();
            facing = direction.getName().substring(0, 1).toUpperCase() + direction.getName().substring(1);
        }
        mc().fontRendererObj.drawStringWithShadow(x, 4, 4, Draw.TEXT);
        mc().fontRendererObj.drawStringWithShadow(y, 4, 15, Draw.TEXT);
        mc().fontRendererObj.drawStringWithShadow(z, 4, 26, Draw.TEXT);
        mc().fontRendererObj.drawStringWithShadow(facing, width() - 4 - mc().fontRendererObj.getStringWidth(facing), 4, Draw.ACCENT);
    }
}
