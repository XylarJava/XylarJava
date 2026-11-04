package io.xylarjava.newlaunch.legacy.utils.api;

import java.net.URL;

/**
 * Represents a texture from the Mojang API.
 */
public final class Texture {
    private final URL url;
    private final boolean slim;

    public Texture(URL url, boolean slim) {
        this.url = url;
        this.slim = slim;
    }

    public URL getUrl() {
        return url;
    }

    public boolean isSlim() {
        return slim;
    }
}
