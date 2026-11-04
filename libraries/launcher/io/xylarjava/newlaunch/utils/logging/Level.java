package io.xylarjava.newlaunch.utils.logging;

public enum Level {
    LAUNCHER("Launcher"),
    DEBUG("Debug"),
    INFO("Info"),
    MESSAGE("Message"),
    WARNING("Warning"),
    ERROR("Error", true),
    FATAL("Fatal", true);

    String name;
    boolean stderr;

    Level(String name) {
        this(name, false);
    }

    Level(String name, boolean stderr) {
        this.name = name;
        this.stderr = stderr;
    }
}
