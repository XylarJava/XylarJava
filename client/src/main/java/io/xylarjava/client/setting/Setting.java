package io.xylarjava.client.setting;

/** One option of a module. Values are stored as strings in the config so the file stays readable. */
public abstract class Setting<T> {

    private final String id;
    private final String name;
    protected T value;
    private final T defaultValue;

    protected Setting(String id, String name, T defaultValue) {
        this.id = id;
        this.name = name;
        this.value = defaultValue;
        this.defaultValue = defaultValue;
    }

    public final String id() {
        return id;
    }

    public final String name() {
        return name;
    }

    public T get() {
        return value;
    }

    public void set(T value) {
        this.value = value;
    }

    public void reset() {
        set(defaultValue);
    }

    /** Text shown next to the setting in the menu. */
    public abstract String display();

    public abstract String save();

    /** Reads a saved value; bad input keeps the current value. */
    public abstract void load(String text);
}
