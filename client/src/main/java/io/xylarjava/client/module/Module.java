package io.xylarjava.client.module;

import io.xylarjava.client.config.ClientConfig;
import net.minecraft.client.Minecraft;

/** A feature that can be switched on and off from the mod menu. */
public abstract class Module {

    private final String id;
    private final String name;
    private final String description;
    protected ClientConfig.ModuleState state;

    protected Module(String id, String name, String description) {
        this.id = id;
        this.name = name;
        this.description = description;
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

    /** Whether the module starts enabled on a fresh config. */
    protected boolean enabledByDefault() {
        return true;
    }

    public void attach(ClientConfig config) {
        state = config.state(id, enabledByDefault());
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

    protected static Minecraft mc() {
        return Minecraft.getMinecraft();
    }
}
