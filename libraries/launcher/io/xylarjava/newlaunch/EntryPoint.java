package io.xylarjava.newlaunch;

import io.xylarjava.newlaunch.exception.ParseException;
import io.xylarjava.newlaunch.launcher.Launcher;
import io.xylarjava.newlaunch.launcher.impl.StandardLauncher;
import io.xylarjava.newlaunch.legacy.LegacyProxy;
import io.xylarjava.newlaunch.utils.Parameters;
import io.xylarjava.newlaunch.utils.logging.Log;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

public final class EntryPoint {
    public static void main(String[] args) {
        ExitCode code = listen();

        if (code != ExitCode.NORMAL) {
            Log.fatal("Exiting with " + code);

            System.exit(code.numeric);
        }
    }

    private static ExitCode listen() {
        Parameters params = new Parameters();
        PreLaunchAction action = PreLaunchAction.PROCEED;

        try (BufferedReader reader = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8))) {
            while (action == PreLaunchAction.PROCEED) {
                String line = reader.readLine();
                if (line != null)
                    action = parseLine(line, params);
                else
                    action = PreLaunchAction.ABORT;
            }
        } catch (IllegalArgumentException e) {
            Log.fatal("Aborting due to wrong argument", e);

            return ExitCode.ILLEGAL_ARGUMENT;
        } catch (Throwable e) {
            Log.fatal("Aborting due to exception", e);

            return ExitCode.ABORT;
        }

        if (action == PreLaunchAction.ABORT) {
            Log.fatal("Launch aborted by the launcher");

            return ExitCode.ABORT;
        }

        SystemProperties.apply(params);

        String launcherType = params.getString("launcher");

        try {
            LegacyProxy.applyOnlineFixes(params);

            Launcher launcher;

            switch (launcherType) {
                case "standard":
                    launcher = new StandardLauncher(params);
                    break;

                case "legacy":
                    launcher = LegacyProxy.createLauncher(params);
                    break;

                default:
                    throw new IllegalArgumentException("Invalid launcher type: " + launcherType);
            }

            launcher.launch();

            return ExitCode.NORMAL;
        } catch (IllegalArgumentException e) {
            Log.fatal("Illegal argument", e);

            return ExitCode.ILLEGAL_ARGUMENT;
        } catch (Throwable e) {
            Log.fatal("Exception caught from launcher", e);

            return ExitCode.ERROR;
        }
    }

    private static PreLaunchAction parseLine(String input, Parameters params) throws ParseException {
        switch (input) {
            case "":
                return PreLaunchAction.PROCEED;

            case "launch":
                return PreLaunchAction.LAUNCH;

            case "abort":
                return PreLaunchAction.ABORT;

            default:
                String[] pair = input.split(" ", 2);

                if (pair.length != 2)
                    throw new ParseException(input, "[key] [value]");

                params.add(pair[0], pair[1]);

                return PreLaunchAction.PROCEED;
        }
    }

    private enum PreLaunchAction { PROCEED, LAUNCH, ABORT }

    private enum ExitCode {
        NORMAL(0),
        ABORT(1),
        ERROR(2),
        ILLEGAL_ARGUMENT(65);

        private final int numeric;

        ExitCode(int numeric) {
            this.numeric = numeric;
        }
    }
}
