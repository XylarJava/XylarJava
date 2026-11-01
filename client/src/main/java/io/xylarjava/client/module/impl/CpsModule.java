package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.util.ClickTracker;

public class CpsModule extends HudModule {

    public CpsModule() {
        super("cps", "CPS", "Clicks per second (left | right).", 0.01f, 0.06f);
    }

    @Override
    public int width() {
        return 64;
    }

    @Override
    public int height() {
        return 16;
    }

    @Override
    protected void draw(boolean editing) {
        drawLabel(ClickTracker.leftCps() + " | " + ClickTracker.rightCps() + " CPS");
    }
}
