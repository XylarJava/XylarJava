package io.xylarjava.newlaunch.legacy;

import io.xylarjava.newlaunch.launcher.Launcher;
import io.xylarjava.newlaunch.utils.Parameters;

// used as a fallback if NewLaunchLegacy is not on the classpath
// if it is, this class will be replaced
public final class LegacyProxy {
    public static Launcher createLauncher(Parameters params) {
        throw new AssertionError("NewLaunchLegacy is not loaded");
    }

    public static void applyOnlineFixes(Parameters params) {}
}
