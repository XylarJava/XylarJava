package io.xylarjava.newlaunch.legacy;

import io.xylarjava.newlaunch.launcher.Launcher;
import io.xylarjava.newlaunch.legacy.fix.online.OnlineFixes;
import io.xylarjava.newlaunch.utils.Parameters;

// implementation of LegacyProxy
public final class LegacyProxy {
    public static Launcher createLauncher(Parameters params) {
        return new LegacyLauncher(params);
    }

    public static void applyOnlineFixes(Parameters parameters) {
        OnlineFixes.apply(parameters);
    }
}
