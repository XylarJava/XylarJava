package io.xylarjava.newlaunch.launcher.impl;

import io.xylarjava.newlaunch.utils.Parameters;
import io.xylarjava.newlaunch.utils.ReflectionUtils;

import java.lang.invoke.MethodHandle;
import java.util.Collections;
import java.util.List;

public final class StandardLauncher extends AbstractLauncher {
    private final boolean quickPlayMultiplayerSupported;
    private final boolean quickPlaySingleplayerSupported;

    public StandardLauncher(Parameters params) {
        super(params);

        List<String> traits = params.getList("traits", Collections.<String>emptyList());
        quickPlayMultiplayerSupported = traits.contains("feature:is_quick_play_multiplayer");
        quickPlaySingleplayerSupported = traits.contains("feature:is_quick_play_singleplayer");
    }

    @Override
    public void launch() throws Throwable {
        // window size, title and state
        gameArgs.add("--width");
        gameArgs.add(Integer.toString(width));
        gameArgs.add("--height");
        gameArgs.add(Integer.toString(height));

        if (serverAddress != null) {
            if (quickPlayMultiplayerSupported) {
                // as of 23w14a
                gameArgs.add("--quickPlayMultiplayer");
                gameArgs.add(serverAddress + ':' + serverPort);
            } else {
                gameArgs.add("--server");
                gameArgs.add(serverAddress);
                gameArgs.add("--port");
                gameArgs.add(serverPort);
            }
        } else if (worldName != null && quickPlaySingleplayerSupported) {
            gameArgs.add("--quickPlaySingleplayer");
            gameArgs.add(worldName);
        }

        StringBuilder joinedGameArgs = new StringBuilder();
        for (String gameArg : gameArgs) {
            if (joinedGameArgs.length() > 0) {
                joinedGameArgs.append('\u001F'); // unit separator, designed for this purpose
            }
            joinedGameArgs.append(gameArg);
        }

        // pass the real main class and game arguments in so mods can access them
        System.setProperty("io.xylarjava.newlaunch.launch.mainclass", mainClassName);
        // unit separator ('\u001F') delimited list of game args
        System.setProperty("io.xylarjava.newlaunch.launch.gameargs", joinedGameArgs.toString());

        // find and invoke the main method
        MethodHandle method = ReflectionUtils.findMainMethod(mainClassName);
        method.invokeExact(gameArgs.toArray(new String[0]));
    }
}
