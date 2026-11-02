package io.xylarjava.client;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Properties;

import org.lwjgl.input.Keyboard;

import io.xylarjava.client.config.ClientConfig;
import io.xylarjava.client.gui.ModMenuScreen;
import io.xylarjava.client.module.HudModule;
import io.xylarjava.client.module.Module;
import io.xylarjava.client.util.CombatTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.ScaledResolution;
import net.minecraft.client.renderer.GlStateManager;

/** The client's state: config, modules, and the hooks the mixins call into. */
public final class XylarClient {

    public static final String NAME = "XylarJava";

    private static XylarClient instance;

    private final ClientConfig config;
    private final List<Module> modules = new ArrayList<Module>();
    private final String version;
    private boolean menuKeyWasDown;
    private int saveCountdown;

    public static XylarClient get() {
        if (instance == null)
            instance = new XylarClient();
        return instance;
    }

    private XylarClient() {
        Minecraft mc = Minecraft.getMinecraft();
        config = ClientConfig.load(mc.mcDataDir);
        version = readVersion();

        modules.addAll(Modules.all());
        for (Module module : modules)
            module.attach(config);
        writeConfig();
    }

    public ClientConfig config() {
        return config;
    }

    public List<Module> modules() {
        return Collections.unmodifiableList(modules);
    }

    public String version() {
        return version;
    }

    /** Ask for the config to be written soon (batched, so dragging HUD items doesn't hit the disk every frame). */
    public void markDirty() {
        saveCountdown = 20;
    }

    public void saveNow() {
        saveCountdown = 0;
        writeConfig();
    }

    private void writeConfig() {
        for (Module module : modules)
            module.storeSettings();
        config.save();
    }

    // ---- hooks --------------------------------------------------------------

    /** End of every client tick. */
    public void onTick(Minecraft mc) {
        for (Module module : modules) {
            if (module.isEnabled())
                module.onTick(mc);
        }

        // Right Shift (configurable) opens the mod menu while playing.
        boolean menuKeyDown = Keyboard.isKeyDown(config.menuKey);
        if (menuKeyDown && !menuKeyWasDown && mc.currentScreen == null && mc.theWorld != null)
            mc.displayGuiScreen(new ModMenuScreen(null));
        menuKeyWasDown = menuKeyDown;

        CombatTracker.tick(mc);

        if (saveCountdown > 0 && --saveCountdown == 0)
            writeConfig();
    }

    /** After the vanilla HUD has been drawn. */
    public void renderHud() {
        Minecraft mc = Minecraft.getMinecraft();
        if (mc.gameSettings.hideGUI || mc.gameSettings.showDebugInfo || mc.theWorld == null)
            return;
        ScaledResolution res = new ScaledResolution(mc);
        GlStateManager.enableBlend();
        if (Modules.CROSSHAIR.isEnabled())
            Modules.CROSSHAIR.draw(mc, res);
        for (Module module : modules) {
            if (module instanceof HudModule && module.isEnabled())
                ((HudModule) module).render(res, false);
        }
        GlStateManager.color(1f, 1f, 1f, 1f);
    }

    public float modifyFov(float fov) {
        return Modules.ZOOM.apply(fov);
    }

    private static String readVersion() {
        InputStream in = XylarClient.class.getResourceAsStream("/xylarjava.properties");
        if (in == null)
            return "dev";
        try {
            Properties properties = new Properties();
            properties.load(in);
            return properties.getProperty("version", "dev");
        } catch (Exception e) {
            return "dev";
        } finally {
            try {
                in.close();
            } catch (Exception ignored) {
            }
        }
    }
}
