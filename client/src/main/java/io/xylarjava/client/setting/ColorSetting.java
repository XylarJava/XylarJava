package io.xylarjava.client.setting;

/** An ARGB colour picked from a palette; clicking cycles through it. */
public class ColorSetting extends Setting<Integer> {

    public static final int[] PALETTE = {
        0xFFFF9A3C, 0xFFFFFFFF, 0xFFE9EBF3, 0xFF8E94A8, 0xFF101218, 0xFFFF4F5E, 0xFFFF7AB6, 0xFFB36BFF,
        0xFF6B8CFF, 0xFF3CC8FF, 0xFF3CFFB4, 0xFF7CFF3C, 0xFFFFE03C, 0xFFFFD14A, 0xFF000000,
    };
    public static final String[] NAMES = {
        "Orange", "White", "Light grey", "Grey", "Night", "Red", "Pink", "Purple",
        "Blue", "Sky", "Mint", "Lime", "Yellow", "Gold", "Black",
    };

    public ColorSetting(String id, String name, int defaultValue) {
        super(id, name, defaultValue);
    }

    public int argb() {
        return value;
    }

    public int rgb() {
        return value & 0xFFFFFF;
    }

    public void cycle(boolean forward) {
        int index = 0;
        for (int i = 0; i < PALETTE.length; i++) {
            if (PALETTE[i] == value) {
                index = i;
                break;
            }
        }
        index = (index + (forward ? 1 : PALETTE.length - 1)) % PALETTE.length;
        set(PALETTE[index]);
    }

    @Override
    public String display() {
        for (int i = 0; i < PALETTE.length; i++) {
            if (PALETTE[i] == value)
                return NAMES[i];
        }
        return String.format("#%06X", value & 0xFFFFFF);
    }

    @Override
    public String save() {
        return String.format("#%08X", value);
    }

    @Override
    public void load(String text) {
        if (text == null || !text.startsWith("#"))
            return;
        try {
            set((int) Long.parseLong(text.substring(1), 16));
        } catch (NumberFormatException ignored) {
        }
    }
}
