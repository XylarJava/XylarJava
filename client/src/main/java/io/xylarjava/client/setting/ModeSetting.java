package io.xylarjava.client.setting;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** One choice out of a short list; clicking cycles through them. */
public class ModeSetting extends Setting<String> {

    private final List<String> modes;

    public ModeSetting(String id, String name, String defaultValue, String... modes) {
        super(id, name, defaultValue);
        this.modes = Collections.unmodifiableList(Arrays.asList(modes));
    }

    public List<String> modes() {
        return modes;
    }

    public boolean is(String mode) {
        return mode.equals(value);
    }

    public int index() {
        return Math.max(0, modes.indexOf(value));
    }

    public void cycle(boolean forward) {
        int next = (index() + (forward ? 1 : modes.size() - 1)) % modes.size();
        set(modes.get(next));
    }

    @Override
    public String display() {
        return value;
    }

    @Override
    public String save() {
        return value;
    }

    @Override
    public void load(String text) {
        if (modes.contains(text))
            set(text);
    }
}
