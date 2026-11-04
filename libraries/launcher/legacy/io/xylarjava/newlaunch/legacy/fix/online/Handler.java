package io.xylarjava.newlaunch.legacy.fix.online;

import io.xylarjava.newlaunch.legacy.utils.url.UrlUtils;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;

final class Handler extends URLStreamHandler {
    @Override
    protected URLConnection openConnection(URL address) throws IOException {
        return openConnection(address, null);
    }

    @Override
    protected URLConnection openConnection(URL address, Proxy proxy) throws IOException {
        URLConnection result;

        // try various fixes...
        result = SkinFix.openConnection(address, proxy);
        if (result != null)
            return result;

        result = OnlineModeFix.openConnection(address, proxy);
        if (result != null)
            return result;

        // ...then give up and make the request directly
        return UrlUtils.openConnection(address, proxy);
    }
}
