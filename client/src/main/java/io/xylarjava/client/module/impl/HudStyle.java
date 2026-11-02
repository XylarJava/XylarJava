package io.xylarjava.client.module.impl;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ColorSetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;

/** Shared look of every HUD element. When it's switched off, the defaults are used. */
public class HudStyle extends Module {

    private final BoolSetting background = bool("background", "Background", true);
    private final NumberSetting opacity = number("opacity", "Background opacity", 60f, 0f, 100f, 5f, "%");
    private final BoolSetting shadow = bool("shadow", "Text shadow", true);
    private final ModeSetting brackets = mode("brackets", "Brackets", "None", "None", "[ ]", "( )", "< >");
    private final ColorSetting text = color("text", "Text colour", 0xFFE9EBF3);

    public HudStyle() {
        super("hudstyle", "HUD Style", "Background, shadow and colours of the HUD.", Category.HUD);
    }

    @Override
    protected boolean enabledByDefault() {
        return true;
    }

    public boolean background() {
        return !isEnabled() || background.on();
    }

    public int backgroundColor() {
        float alpha = isEnabled() ? opacity.value() / 100f : 0.6f;
        return (Math.round(alpha * 255f) << 24) | 0x101218;
    }

    public boolean shadow() {
        return !isEnabled() || shadow.on();
    }

    public int textColor() {
        return isEnabled() ? text.argb() : 0xFFE9EBF3;
    }

    public String decorate(String value) {
        if (!isEnabled() || brackets.is("None"))
            return value;
        String b = brackets.get();
        return b.charAt(0) + value + b.charAt(2);
    }
}
