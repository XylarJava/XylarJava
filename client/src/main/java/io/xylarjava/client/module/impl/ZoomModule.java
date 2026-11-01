package io.xylarjava.client.module.impl;

import org.lwjgl.input.Keyboard;

import io.xylarjava.client.XylarClient;
import io.xylarjava.client.module.Module;
import net.minecraft.client.Minecraft;

/** Hold the zoom key (C by default) to zoom in, like OptiFine. */
public class ZoomModule extends Module {

    private boolean zooming;
    private boolean savedSmoothCamera;

    public ZoomModule() {
        super("zoom", "Zoom", "Hold C to zoom in.");
    }

    public boolean isZooming() {
        return isEnabled() && zooming;
    }

    @Override
    public void onTick(Minecraft mc) {
        boolean held = mc.currentScreen == null && Keyboard.isKeyDown(XylarClient.get().config().zoomKey);
        if (held == zooming)
            return;
        zooming = held;
        if (XylarClient.get().config().smoothZoom) {
            if (zooming) {
                savedSmoothCamera = mc.gameSettings.smoothCamera;
                mc.gameSettings.smoothCamera = true;
            } else {
                mc.gameSettings.smoothCamera = savedSmoothCamera;
            }
        }
    }

    @Override
    protected void onDisable() {
        if (zooming && XylarClient.get().config().smoothZoom)
            mc().gameSettings.smoothCamera = savedSmoothCamera;
        zooming = false;
    }

    public float apply(float fov) {
        return isZooming() ? fov / Math.max(1f, XylarClient.get().config().zoomFactor) : fov;
    }
}
