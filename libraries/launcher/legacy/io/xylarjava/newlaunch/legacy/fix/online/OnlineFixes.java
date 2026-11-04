package io.xylarjava.newlaunch.legacy.fix.online;

import io.xylarjava.newlaunch.legacy.utils.Base64;
import io.xylarjava.newlaunch.legacy.utils.url.UrlUtils;
import io.xylarjava.newlaunch.utils.Parameters;
import io.xylarjava.newlaunch.utils.logging.Log;

import java.net.URL;
import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;

/**
 * Fixes skins by redirecting to other URLs.
 * Thanks to MineOnline for the implementation from which this was inspired!
 * See https://github.com/ahnewark/MineOnline/tree/main/src/main/java/gg/codie/mineonline/protocol.
 *
 * @see {@link Handler}
 * @see {@link UrlUtils}
 */
public final class OnlineFixes implements URLStreamHandlerFactory {
    public static void apply(Parameters params) {
        if (!"true".equals(params.getString("onlineFixes", null)))
            return;

        if (!UrlUtils.isSupported() || !Base64.isSupported()) {
            Log.warning("Cannot access the necessary Java internals for skin fix");
            Log.warning("Turning off online fixes in the settings will silence the warnings");
            return;
        }

        try {
            URL.setURLStreamHandlerFactory(new OnlineFixes());
        } catch (Error e) {
            Log.warning("Cannot apply skin fix: URLStreamHandlerFactory is already set");
            Log.warning("Turning off online fixes in the settings will silence the warnings");
        }
    }

    @Override
    public URLStreamHandler createURLStreamHandler(String protocol) {
        if ("http".equals(protocol))
            return new Handler();

        return null;
    }
}
