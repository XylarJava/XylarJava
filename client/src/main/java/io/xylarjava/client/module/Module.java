package io.xylarjava.client.module;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import io.xylarjava.client.config.ClientConfig;
import io.xylarjava.client.setting.BoolSetting;
import io.xylarjava.client.setting.ColorSetting;
import io.xylarjava.client.setting.KeySetting;
import io.xylarjava.client.setting.ModeSetting;
import io.xylarjava.client.setting.NumberSetting;
import io.xylarjava.client.setting.Setting;
import net.minecraft.client.Minecraft;

/** A feature that can be switched on and off from the mod menu, with its own settings. */
public abstract class Module {

    private final String id;
    private final String name;
    private final String description;
    private final Category category;
    private final List<Setting<?>> settings = new ArrayList<Setting<?>>();
    protected ClientConfig.ModuleState state;

    protected Module(String id, String name, String description, Category category) {
        this.id = id;
        this.name = name;
        this.description = description;
        this.category = category;
    }

    public final String id() {
        return id;
    }

    public final String name() {
        return name;
    }

    public final String description() {
        return description;
    }

    public final Category category() {
        return category;
    }

    public final List<Setting<?>> settings() {
        return Collections.unmodifiableList(settings);
    }

    /** Whether the module starts enabled on a fresh config. */
    protected boolean enabledByDefault() {
        return false;
    }

    public void attach(ClientConfig config) {
        state = config.state(id, enabledByDefault());
        for (Setting<?> setting : settings) {
            String saved = state.settings.get(setting.id());
            if (saved != null)
                setting.load(saved);
        }
    }

    /** Copies the current setting values into the config (called before it is written). */
    public void storeSettings() {
        if (state == null)
            return;
        for (Setting<?> setting : settings)
            state.settings.put(setting.id(), setting.save());
    }

    public boolean isEnabled() {
        return state != null && state.enabled;
    }

    public void setEnabled(boolean enabled) {
        if (state == null || state.enabled == enabled)
            return;
        state.enabled = enabled;
        if (enabled)
            onEnable();
        else
            onDisable();
    }

    public void toggle() {
        setEnabled(!isEnabled());
    }

    protected void onEnable() {}

    protected void onDisable() {}

    /** Called once per client tick (20 times a second) while enabled. */
    public void onTick(Minecraft mc) {}

    // ---- settings helpers ---------------------------------------------------

    protected final <S extends Setting<?>> S add(S setting) {
        settings.add(setting);
        return setting;
    }

    protected final BoolSetting bool(String id, String name, boolean defaultValue) {
        return add(new BoolSetting(id, name, defaultValue));
    }

    protected final NumberSetting number(String id, String name, float defaultValue, float min, float max, float step, String suffix) {
        return add(new NumberSetting(id, name, defaultValue, min, max, step, suffix));
    }

    protected final ModeSetting mode(String id, String name, String defaultValue, String... modes) {
        return add(new ModeSetting(id, name, defaultValue, modes));
    }

    protected final ColorSetting color(String id, String name, int defaultValue) {
        return add(new ColorSetting(id, name, defaultValue));
    }

    protected final KeySetting key(String id, String name, int defaultKey) {
        return add(new KeySetting(id, name, defaultKey));
    }

    protected static Minecraft mc() {
        return Minecraft.getMinecraft();
    }
}
