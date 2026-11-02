package io.xylarjava.client.module;

import io.xylarjava.client.Modules;

/** A HUD element that shows one line of text; its width follows the text. */
public abstract class TextHudModule extends HudModule {

    protected TextHudModule(String id, String name, String description, float defaultX, float defaultY) {
        super(id, name, description, defaultX, defaultY);
    }

    /** The text to show; may change every frame. */
    protected abstract String text();

    @Override
    public int width() {
        return Math.max(24, mc().fontRendererObj.getStringWidth(Modules.HUD_STYLE.decorate(text())) + 8);
    }

    @Override
    public int height() {
        return 16;
    }

    @Override
    protected void draw(boolean editing) {
        drawLabel(text());
    }
}
