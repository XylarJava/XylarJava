package io.xylarjava.client.module.impl;

import org.lwjgl.input.Mouse;

import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.util.ClickTracker;
import io.xylarjava.client.util.Draw;
import net.minecraft.client.settings.GameSettings;
import net.minecraft.client.settings.KeyBinding;

/** W A S D, the mouse buttons with CPS, and the space bar. */
public class KeystrokesModule extends HudModule {

    private static final int KEY = 22;
    private static final int GAP = 2;

    public KeystrokesModule() {
        super("keystrokes", "Keystrokes", "Shows WASD and mouse clicks.", 0.01f, 0.2f);
    }

    @Override
    public int width() {
        return KEY * 3 + GAP * 2;
    }

    @Override
    public int height() {
        return KEY * 2 + GAP * 3 + 18 + 10;
    }

    @Override
    protected void draw(boolean editing) {
        GameSettings s = mc().gameSettings;
        key(KEY + GAP, 0, KEY, KEY, keyName(s.keyBindForward), s.keyBindForward.isKeyDown());
        key(0, KEY + GAP, KEY, KEY, keyName(s.keyBindLeft), s.keyBindLeft.isKeyDown());
        key(KEY + GAP, KEY + GAP, KEY, KEY, keyName(s.keyBindBack), s.keyBindBack.isKeyDown());
        key((KEY + GAP) * 2, KEY + GAP, KEY, KEY, keyName(s.keyBindRight), s.keyBindRight.isKeyDown());

        int mouseY = (KEY + GAP) * 2;
        int half = (width() - GAP) / 2;
        key(0, mouseY, half, 18, ClickTracker.leftCps() + " CPS", Mouse.isButtonDown(0));
        key(half + GAP, mouseY, width() - half - GAP, 18, ClickTracker.rightCps() + " CPS", Mouse.isButtonDown(1));
        key(0, mouseY + 18 + GAP, width(), 8, "", s.keyBindJump.isKeyDown());
    }

    private void key(int x, int y, int w, int h, String label, boolean pressed) {
        Draw.rect(x, y, w, h, pressed ? Draw.PRESSED : Draw.PANEL);
        if (!label.isEmpty()) {
            int textWidth = mc().fontRendererObj.getStringWidth(label);
            mc().fontRendererObj.drawString(label, x + (w - textWidth) / 2, y + (h - 8) / 2, pressed ? 0xFF101218 : Draw.TEXT);
        } else if (pressed) {
            Draw.rect(x + w / 2 - 10, y + h / 2, 20, 1, 0xFF101218);
        }
    }

    private static String keyName(KeyBinding binding) {
        String name = GameSettings.getKeyDisplayString(binding.getKeyCode());
        return name.length() > 3 ? name.substring(0, 3) : name;
    }
}
