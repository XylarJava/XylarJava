package io.xylarjava.client.module.impl;

import java.awt.image.BufferedImage;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.regex.Pattern;

import javax.imageio.ImageIO;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import io.xylarjava.client.module.Category;
import io.xylarjava.client.module.Module;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.util.ResourceLocation;

/**
 * XylarJava capes on every server: each player's cape is looked up by name on the XylarJava cape
 * backend (the one the launcher uses, from -Dxylarjava.capes) and drawn in place of the vanilla
 * one. Lookups run off the render thread and are remembered for a few minutes.
 */
public class CapesModule extends Module {

    private static final String DEFAULT_BACKEND = "https://xylarjava-capes.xylarinc.workers.dev";
    private static final Pattern PLAYER_NAME = Pattern.compile("[A-Za-z0-9_]{1,16}");
    private static final Pattern TEXTURE_HASH = Pattern.compile("[0-9a-f]{64}");
    /** How long an answer is kept: a changed cape shows up after this. */
    private static final long REFRESH_MS = 5 * 60 * 1000L;
    /** After a network error: try again sooner. */
    private static final long RETRY_MS = 60 * 1000L;

    private final String backend;
    private final ConcurrentHashMap<String, Entry> entries = new ConcurrentHashMap<String, Entry>();
    private final ExecutorService lookups = Executors.newFixedThreadPool(2, new ThreadFactory() {
        @Override
        public Thread newThread(Runnable runnable) {
            Thread thread = new Thread(runnable, "XylarJava capes");
            thread.setDaemon(true);
            return thread;
        }
    });

    /** One player's cape. Written by the lookup threads and the main thread, read while rendering. */
    private static final class Entry {
        volatile ResourceLocation location;
        volatile String hash;
        volatile long refreshAt;
        volatile boolean loading;
    }

    public CapesModule() {
        super("capes", "XylarJava Capes", "Show the capes players picked in XylarJava, on every server.", Category.VISUAL);
        String configured = System.getProperty("xylarjava.capes", DEFAULT_BACKEND).trim();
        while (configured.endsWith("/"))
            configured = configured.substring(0, configured.length() - 1);
        backend = configured.isEmpty() ? DEFAULT_BACKEND : configured;
    }

    @Override
    protected boolean enabledByDefault() {
        return true;
    }

    /** The cape texture for this player, or null to keep the vanilla one. Called while rendering. */
    public ResourceLocation capeFor(String name) {
        if (!isEnabled() || name == null || !PLAYER_NAME.matcher(name).matches())
            return null;
        Entry entry = entries.get(name);
        if (entry == null) {
            entry = new Entry();
            Entry existing = entries.putIfAbsent(name, entry);
            if (existing != null)
                entry = existing;
        }
        if (!entry.loading && System.currentTimeMillis() >= entry.refreshAt) {
            entry.loading = true;
            lookup(name, entry);
        }
        return entry.location;
    }

    private void lookup(final String name, final Entry entry) {
        lookups.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    String hash = capeHash(name);
                    if (hash == null) {
                        show(entry, null, null);
                    } else if (hash.equals(entry.hash) && entry.location != null) {
                        entry.refreshAt = System.currentTimeMillis() + REFRESH_MS;
                        entry.loading = false;
                    } else {
                        show(entry, hash, download(hash));
                    }
                } catch (Exception e) {
                    entry.refreshAt = System.currentTimeMillis() + RETRY_MS;
                    entry.loading = false;
                }
            }
        });
    }

    /** The hash of the cape this player wears, or null. */
    private String capeHash(String name) throws Exception {
        HttpURLConnection connection = open(backend + "/api/cape/by-name/" + name);
        try {
            if (connection.getResponseCode() != 200)
                return null;
            InputStreamReader reader = new InputStreamReader(connection.getInputStream(), "UTF-8");
            try {
                JsonElement root = new JsonParser().parse(reader);
                if (!root.isJsonObject())
                    return null;
                JsonElement cape = ((JsonObject) root).get("cape_hash");
                if (cape == null || !cape.isJsonPrimitive())
                    return null;
                String hash = cape.getAsString().toLowerCase();
                return TEXTURE_HASH.matcher(hash).matches() ? hash : null;
            } finally {
                reader.close();
            }
        } finally {
            connection.disconnect();
        }
    }

    private BufferedImage download(String hash) throws Exception {
        HttpURLConnection connection = open(backend + "/api/cape-texture/" + hash);
        try {
            if (connection.getResponseCode() != 200)
                throw new IllegalStateException("cape texture: HTTP " + connection.getResponseCode());
            InputStream in = connection.getInputStream();
            try {
                BufferedImage image = ImageIO.read(in);
                if (image == null)
                    throw new IllegalStateException("cape texture is not an image");
                return image;
            } finally {
                in.close();
            }
        } finally {
            connection.disconnect();
        }
    }

    private static HttpURLConnection open(String url) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
        connection.setConnectTimeout(5000);
        connection.setReadTimeout(8000);
        // Cloudflare turns away Java's own User-Agent.
        connection.setRequestProperty("User-Agent", "XylarJava");
        return connection;
    }

    /** Swaps the texture on the main thread, where OpenGL calls are allowed. */
    private static void show(final Entry entry, final String hash, final BufferedImage image) {
        final Minecraft mc = Minecraft.getMinecraft();
        mc.addScheduledTask(new Runnable() {
            @Override
            public void run() {
                ResourceLocation old = entry.location;
                entry.location = image == null ? null
                        : mc.getTextureManager().getDynamicTextureLocation("xylarjava_cape", new DynamicTexture(image));
                entry.hash = hash;
                if (old != null)
                    mc.getTextureManager().deleteTexture(old);
                entry.refreshAt = System.currentTimeMillis() + REFRESH_MS;
                entry.loading = false;
            }
        });
    }
}
