package io.xylarjava.client.setting;

import org.lwjgl.input.Keyboard;

/** A keyboard key (LWJGL key code). 0 means unbound. */
public class KeySetting extends Setting<Integer> {

    public KeySetting(String id, String name, int defaultKey) {
        super(id, name, defaultKey);
    }

    public int key() {
        return value;
    }

    public boolean isDown() {
        return value > 0 && Keyboard.isKeyDown(value);
    }

    @Override
    public String display() {
        if (value <= 0)
            return "None";
        String name = Keyboard.getKeyName(value);
        return name == null ? "Key " + value : name;
    }

    @Override
    public String save() {
        return String.valueOf(value);
    }

    @Override
    public void load(String text) {
        try {
            set(Integer.parseInt(text));
        } catch (NumberFormatException ignored) {
        }
    }
}
