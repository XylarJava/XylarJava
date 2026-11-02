package io.xylarjava.client.setting;

/** A number on a slider, kept between min and max and snapped to step. */
public class NumberSetting extends Setting<Float> {

    private final float min;
    private final float max;
    private final float step;
    private final String suffix;

    public NumberSetting(String id, String name, float defaultValue, float min, float max, float step, String suffix) {
        super(id, name, defaultValue);
        this.min = min;
        this.max = max;
        this.step = step;
        this.suffix = suffix;
    }

    public float min() {
        return min;
    }

    public float max() {
        return max;
    }

    public float value() {
        return value;
    }

    public int intValue() {
        return Math.round(value);
    }

    /** Position on the slider, 0..1. */
    public float fraction() {
        return max > min ? (value - min) / (max - min) : 0f;
    }

    public void setFraction(float fraction) {
        set(min + Math.max(0f, Math.min(1f, fraction)) * (max - min));
    }

    @Override
    public void set(Float v) {
        float clamped = Math.max(min, Math.min(max, v));
        if (step > 0f)
            clamped = min + Math.round((clamped - min) / step) * step;
        super.set(Math.max(min, Math.min(max, clamped)));
    }

    @Override
    public String display() {
        String number = step >= 1f ? String.valueOf(Math.round(value)) : String.format(java.util.Locale.ROOT, "%.1f", value);
        return number + suffix;
    }

    @Override
    public String save() {
        return String.valueOf(value);
    }

    @Override
    public void load(String text) {
        try {
            set(Float.parseFloat(text));
        } catch (NumberFormatException ignored) {
        }
    }
}
