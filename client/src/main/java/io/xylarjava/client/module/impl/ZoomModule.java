package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.KeySetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;
import net.minecraft.client.Minecraft;

/** Hold (or toggle) the zoom key, C by default, to zoom in like OptiFine. Scroll to zoom further. */
public class ZoomModule extends Module {

    private final KeySetting key = key("key", "Zoom key", 46);
    private final ModeSetting mode = mode("mode", "Mode", "Hold", "Hold", "Toggle");
    private final NumberSetting factor = number("factor", "Zoom", 4f, 2f, 10f, 0.5f, "x");
    private final BoolSetting smooth = bool("smooth", "Smooth camera", true);
    private final BoolSetting scroll = bool("scroll", "Scroll to adjust", true);

    private boolean zooming;
    private boolean keyWasDown;
    private boolean savedSmoothCamera;
    private float scrollFactor = 1f;

    public ZoomModule() {
        super("zoom", "Zoom", "Hold C to zoom in.", Category.VISUAL);
    }

    @Override
    protected boolean enabledByDefault() {
        return true;
    }

    public boolean isZooming() {
        return isEnabled() && zooming;
    }

    @Override
    public void onTick(Minecraft mc) {
        boolean down = mc.currentScreen == null && key.isDown();
        boolean next = zooming;
        if (mode.is("Hold"))
            next = down;
        else if (down && !keyWasDown)
            next = !zooming;
        keyWasDown = down;
        if (next == zooming)
            return;
        zooming = next;
        scrollFactor = 1f;
        if (smooth.on()) {
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
        if (zooming && smooth.on())
            mc().gameSettings.smoothCamera = savedSmoothCamera;
        zooming = false;
    }

    /** Mouse wheel while zooming: returns true when the scroll was used for zoom. */
    public boolean onScroll(int direction) {
        if (!isZooming() || !scroll.on())
            return false;
        scrollFactor = Math.max(0.5f, Math.min(4f, scrollFactor * (direction > 0 ? 0.8f : 1.25f)));
        return true;
    }

    public float apply(float fov) {
        return isZooming() ? fov / Math.max(1f, factor.value() / scrollFactor) : fov;
    }
}
