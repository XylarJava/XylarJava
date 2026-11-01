package io.xylarjava.client.config;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Reader;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

/**
 * Everything the client remembers, stored as JSON in {@code <gameDir>/xylarjava/config.json}.
 * Gson ships with Minecraft 1.8.9, so no extra library is needed.
 */
public class ClientConfig {

    /** Per-module state. */
    public static class ModuleState {
        public boolean enabled;
        /** HUD position as a fraction of the scaled screen (0..1), so it survives resolution changes. */
        public float x = -1f;
        public float y = -1f;
        public float scale = 1f;
    }

    public boolean customMainMenu = true;
    public int menuKey = 54; // Keyboard.KEY_RSHIFT
    public int zoomKey = 46; // Keyboard.KEY_C
    public float zoomFactor = 4f;
    public boolean smoothZoom = true;
    public Map<String, ModuleState> modules = new LinkedHashMap<String, ModuleState>();

    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private transient File file;

    public static ClientConfig load(File gameDir) {
        File file = new File(new File(gameDir, "xylarjava"), "config.json");
        ClientConfig config = null;
        if (file.isFile()) {
            Reader reader = null;
            try {
                reader = new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8);
                config = GSON.fromJson(reader, ClientConfig.class);
            } catch (Exception e) {
                System.err.println("[XylarJava] Couldn't read " + file + ", using defaults: " + e);
            } finally {
                closeQuietly(reader);
            }
        }
        if (config == null)
            config = new ClientConfig();
        if (config.modules == null)
            config.modules = new LinkedHashMap<String, ModuleState>();
        config.file = file;
        return config;
    }

    public ModuleState state(String id, boolean enabledByDefault) {
        ModuleState state = modules.get(id);
        if (state == null) {
            state = new ModuleState();
            state.enabled = enabledByDefault;
            modules.put(id, state);
        }
        return state;
    }

    public void save() {
        if (file == null)
            return;
        Writer writer = null;
        try {
            file.getParentFile().mkdirs();
            writer = new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8);
            GSON.toJson(this, writer);
        } catch (Exception e) {
            System.err.println("[XylarJava] Couldn't save " + file + ": " + e);
        } finally {
            closeQuietly(writer);
        }
    }

    private static void closeQuietly(java.io.Closeable closeable) {
        if (closeable == null)
            return;
        try {
            closeable.close();
        } catch (Exception ignored) {
        }
    }
}
