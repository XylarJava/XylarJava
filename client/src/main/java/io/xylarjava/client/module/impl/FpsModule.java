package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.HudModule;
import net.minecraft.client.Minecraft;

public class FpsModule extends HudModule {

    public FpsModule() {
        super("fps", "FPS", "Frames per second.", 0.01f, 0.01f);
    }

    @Override
    public int width() {
        return 52;
    }

    @Override
    public int height() {
        return 16;
    }

    @Override
    protected void draw(boolean editing) {
        drawLabel(Minecraft.getDebugFPS() + " FPS");
    }
}
