package io.xylarjava.newlaunch.launcher.impl;

import io.xylarjava.newlaunch.exception.ParseException;
import io.xylarjava.newlaunch.launcher.Launcher;
import io.xylarjava.newlaunch.utils.Parameters;

import java.util.ArrayList;
import java.util.List;

public abstract class AbstractLauncher implements Launcher {
    private static final int DEFAULT_WINDOW_WIDTH = 854, DEFAULT_WINDOW_HEIGHT = 480;

    // parameters, separated from ParamBucket
    protected final List<String> gameArgs;

    // secondary parameters
    protected final int width, height;
    protected final boolean maximize;
    protected final String serverAddress, serverPort, worldName;

    protected final String mainClassName;

    protected AbstractLauncher(Parameters params) {
        gameArgs = params.getList("param", new ArrayList<String>());
        mainClassName = params.getString("mainClass", "net.minecraft.client.Minecraft");

        serverAddress = params.getString("serverAddress", null);
        serverPort = params.getString("serverPort", null);
        worldName = params.getString("worldName", null);

        String windowParams = params.getString("windowParams", null);

        if ("maximized".equals(windowParams) || windowParams == null) {
            maximize = windowParams != null;

            width = DEFAULT_WINDOW_WIDTH;
            height = DEFAULT_WINDOW_HEIGHT;
        } else {
            maximize = false;

            String[] sizePair = windowParams.split("x", 2);

            if (sizePair.length == 2) {
                try {
                    width = Integer.parseInt(sizePair[0]);
                    height = Integer.parseInt(sizePair[1]);
                    return;
                } catch (NumberFormatException ignored) {
                }
            }

            throw new ParseException(windowParams, "[width]x[height]");
        }
    }
}
