package io.xylarjava.newlaunch.legacy;

import io.xylarjava.newlaunch.launcher.impl.AbstractLauncher;
import io.xylarjava.newlaunch.utils.Parameters;
import io.xylarjava.newlaunch.utils.ReflectionUtils;
import io.xylarjava.newlaunch.utils.logging.Log;

import java.applet.Applet;
import java.io.File;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Collections;
import java.util.List;

/**
 * Used to launch old versions which support applets.
 */
final class LegacyLauncher extends AbstractLauncher {
    private final String user, session;
    private final String title;
    private final String appletClass;
    private final boolean useApplet;
    private final String gameDir;

    public LegacyLauncher(Parameters params) {
        super(params);

        user = params.getString("userName");
        session = params.getString("sessionId");
        title = params.getString("windowTitle", "Minecraft");
        appletClass = params.getString("appletClass", "net.minecraft.client.MinecraftApplet");

        List<String> traits = params.getList("traits", Collections.<String>emptyList());
        useApplet = !traits.contains("noapplet");

        gameDir = System.getProperty("user.dir");
    }

    @Override
    public void launch() throws Throwable {
        Class<?> main = ClassLoader.getSystemClassLoader().loadClass(mainClassName);
        Field gameDirField = findMinecraftGameDirField(main);

        if (gameDirField != null) {
            gameDirField.setAccessible(true);
            gameDirField.set(null, new File(gameDir));
        }

        if (useApplet) {
            System.setProperty("minecraft.applet.TargetDirectory", gameDir);

            try {
                LegacyFrame window = new LegacyFrame(title, createAppletClass(appletClass));

                window.start(user, session, width, height, maximize, serverAddress, serverPort, gameArgs.contains("--demo"));
                return;
            } catch (Throwable e) {
                Log.error("Running applet wrapper failed with exception; falling back to main class", e);
            }
        }

        // find and invoke the main method, this time without size parameters - in all
        // versions that support applets, these are ignored
        MethodHandle method = ReflectionUtils.findMainMethod(main);
        method.invokeExact(gameArgs.toArray(new String[0]));
    }

    private static Applet createAppletClass(String clazz) throws Throwable {
        Class<?> appletClass = ClassLoader.getSystemClassLoader().loadClass(clazz);

        MethodHandle appletConstructor = MethodHandles.lookup().findConstructor(appletClass, MethodType.methodType(void.class));
        return (Applet) appletConstructor.invoke();
    }

    private static Field findMinecraftGameDirField(Class<?> clazz) {
        // search for private static File
        for (Field field : clazz.getDeclaredFields()) {
            if (field.getType() != File.class)
                continue;

            int fieldModifiers = field.getModifiers();

            if (!Modifier.isStatic(fieldModifiers))
                continue;

            if (!Modifier.isPrivate(fieldModifiers))
                continue;

            if (Modifier.isFinal(fieldModifiers))
                continue;

            return field;
        }

        return null;
    }
}
