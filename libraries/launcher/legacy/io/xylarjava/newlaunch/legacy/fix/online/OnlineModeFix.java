package io.xylarjava.newlaunch.legacy.fix.online;

import io.xylarjava.newlaunch.legacy.utils.url.UrlUtils;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;

public final class OnlineModeFix {
    public static URLConnection openConnection(URL address, Proxy proxy) throws IOException {
        // we start with "http://www.minecraft.net/game/joinserver.jsp?user=..."
        if (!(address.getHost().equals("www.minecraft.net") && address.getPath().equals("/game/joinserver.jsp")))
            return null;

        // change it to "https://session.minecraft.net/game/joinserver.jsp?user=..."
        // this seems to be the modern version of the same endpoint...
        // maybe Mojang planned to patch old versions of the game to use it
        // if it ever disappears this should be changed to use sessionserver.mojang.com/session/minecraft/join
        // which of course has a different usage requiring JSON serialisation...
        URL url;
        try {
            url = new URL("https", "session.minecraft.net", address.getPort(), address.getFile());
        } catch (MalformedURLException e) {
            throw new AssertionError("url should be valid", e);
        }

        return UrlUtils.openConnection(url, proxy);
    }
}
