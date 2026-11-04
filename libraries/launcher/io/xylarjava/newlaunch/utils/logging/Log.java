package io.xylarjava.newlaunch.utils.logging;

import java.io.PrintStream;

/**
 * Used to print messages with different levels used to colourise the output.
 * Used instead of a logging framework, as the launcher knows how to parse these
 * messages.
 */
public final class Log {
    // original before possibly overridden by MC
    private static final PrintStream OUT = new PrintStream(System.out), ERR = new PrintStream(System.err);
    private static final boolean DEBUG = Boolean.getBoolean("io.xylarjava.newlaunch.debug");

    public static void launcher(String message) {
        log(message, Level.LAUNCHER);
    }

    public static void error(String message) {
        log(message, Level.ERROR);
    }

    public static void debug(String message) {
        log(message, Level.DEBUG);
    }

    public static void warning(String message) {
        log(message, Level.WARNING);
    }

    public static void error(String message, Throwable e) {
        error(message);
        e.printStackTrace(ERR);
    }

    public static void fatal(String message) {
        log(message, Level.FATAL);
    }

    public static void fatal(String message, Throwable e) {
        fatal(message);
        e.printStackTrace(ERR);
    }

    /**
     * Logs a message with the prefix <code>!![LEVEL]!</code>. This is picked up by
     * the log viewer to give it nice colours.
     *
     * @param message The message
     * @param level   The level
     */
    public static void log(String message, Level level) {
        if (!DEBUG && level == Level.DEBUG)
            return;

        String prefix = "!![" + level.name + "]!";
        // prefix first line
        message = prefix + message;
        // prefix subsequent lines
        message = message.replace("\n", "\n" + prefix);

        if (level.stderr)
            ERR.println(message);
        else
            OUT.println(message);
    }
}
