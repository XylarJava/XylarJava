package io.xylarjava.client.setting;

public class BoolSetting extends Setting<Boolean> {

    public BoolSetting(String id, String name, boolean defaultValue) {
        super(id, name, defaultValue);
    }

    public boolean on() {
        return value;
    }

    public void toggle() {
        set(!value);
    }

    @Override
    public String display() {
        return value ? "On" : "Off";
    }

    @Override
    public String save() {
        return String.valueOf(value);
    }

    @Override
    public void load(String text) {
        if ("true".equals(text) || "false".equals(text))
            set(Boolean.valueOf(text));
    }
}
