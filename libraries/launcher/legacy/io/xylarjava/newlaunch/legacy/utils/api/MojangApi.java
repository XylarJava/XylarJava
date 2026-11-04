package io.xylarjava.newlaunch.legacy.utils.api;

import io.xylarjava.newlaunch.legacy.utils.Base64;
import io.xylarjava.newlaunch.legacy.utils.json.JsonParser;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Map;

/**
 * Basic wrapper for Mojang's Minecraft API.
 */
@SuppressWarnings("unchecked")
public final class MojangApi {
    public static String getUuid(String username) throws IOException {
        try (InputStream in = new URL("https://api.minecraftservices.com/minecraft/profile/lookup/name/" + username).openStream()) {
            Map<String, Object> map = (Map<String, Object>) JsonParser.parse(in);
            return (String) map.get("id");
        }
    }

    public static Texture getTexture(String player, String id) throws IOException {
        Map<String, Object> map = getTextures(player);

        if (map != null) {
            map = (Map<String, Object>) map.get(id);
            if (map == null)
                return null;

            URL url = new URL((String) map.get("url"));
            boolean slim = false;

            if (id.equals("SKIN")) {
                map = (Map<String, Object>) map.get("metadata");
                if (map != null && "slim".equals(map.get("model")))
                    slim = true;
            }

            return new Texture(url, slim);
        }

        return null;
    }

    public static Map<String, Object> getTextures(String player) throws IOException {
        try (InputStream profileIn = new URL("https://sessionserver.mojang.com/session/minecraft/profile/" + player).openStream()) {
            Map<String, Object> profile = (Map<String, Object>) JsonParser.parse(profileIn);

            for (Map<String, Object> property : (Iterable<Map<String, Object>>) profile.get("properties")) {
                if (property.get("name").equals("textures")) {
                    Map<String, Object> result =
                            (Map<String, Object>) JsonParser.parse(new String(Base64.decode((String) property.get("value"))));
                    result = (Map<String, Object>) result.get("textures");

                    return result;
                }
            }

            return null;
        }
    }
}
