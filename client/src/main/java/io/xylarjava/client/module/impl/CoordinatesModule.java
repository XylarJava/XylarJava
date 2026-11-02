package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.util.EnumFacing;
import net.minecraft.util.MathHelper;

public class CoordinatesModule extends HudModule {

    public CoordinatesModule() {
        super("coordinates", "Coordinates", "Your position and facing direction.", 0.01f, 0.11f);
    }

    private final io.xylarjava.client.setting.BoolSetting facing = bool("facing", "Show facing", true);

    @Override
    protected boolean enabledByDefault() {
        return true;
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
        drawBackground(width(), height());
        EntityPlayerSP player = mc().thePlayer;
        String x = "X: -", y = "Y: -", z = "Z: -", facing = "";
        if (player != null) {
            x = "X: " + MathHelper.floor_double(player.posX);
            y = "Y: " + MathHelper.floor_double(player.getEntityBoundingBox().minY);
            z = "Z: " + MathHelper.floor_double(player.posZ);
            EnumFacing direction = player.getHorizontalFacing();
            facing = direction.getName().substring(0, 1).toUpperCase() + direction.getName().substring(1);
        }
        drawText(x, 4, 4, textColor());
        drawText(y, 4, 15, textColor());
        drawText(z, 4, 26, textColor());
        if (this.facing.on())
            drawText(facing, width() - 4 - mc().fontRendererObj.getStringWidth(facing), 4, Draw.ACCENT);
    }
}
