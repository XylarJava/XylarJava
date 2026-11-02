package io.xylarjava.client.launch;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import net.minecraft.launchwrapper.ITweaker;
import net.minecraft.launchwrapper.LaunchClassLoader;
import org.spongepowered.asm.launch.MixinBootstrap;
import org.spongepowered.asm.mixin.MixinEnvironment;
import org.spongepowered.asm.mixin.Mixins;

/**
 * Entry point used by LaunchWrapper ({@code --tweakClass io.xylarjava.client.launch.XylarTweaker}).
 * Boots Mixin, registers our mixins and hands control to the vanilla 1.8.9 main class.
 *
 * <p>Keep this class in its own package: LaunchWrapper excludes the tweaker's whole package from its
 * class loader, and the rest of the client (mixins, GUI) must be loaded next to Minecraft's classes.
 */
public class XylarTweaker implements ITweaker {

    private final List<String> launchArguments = new ArrayList<String>();

    @Override
    public void acceptOptions(List<String> args, File gameDir, File assetsDir, String profile) {
        launchArguments.addAll(args);
        // LaunchWrapper consumes these three; vanilla Main still needs them.
        if (!args.contains("--version") && profile != null) {
            launchArguments.add("--version");
            launchArguments.add(profile);
        }
        if (!args.contains("--assetsDir") && assetsDir != null) {
            launchArguments.add("--assetsDir");
            launchArguments.add(assetsDir.getAbsolutePath());
        }
        if (!args.contains("--gameDir") && gameDir != null) {
            launchArguments.add("--gameDir");
            launchArguments.add(gameDir.getAbsolutePath());
        }
    }

    @Override
    public void injectIntoClassLoader(LaunchClassLoader classLoader) {
        MixinBootstrap.init();
        Mixins.addConfiguration("mixins.xylarjava.json");
        MixinEnvironment environment = MixinEnvironment.getDefaultEnvironment();
        environment.setObfuscationContext("notch");
        environment.setSide(MixinEnvironment.Side.CLIENT);
    }

    @Override
    public String getLaunchTarget() {
        return "net.minecraft.client.main.Main";
    }

    @Override
    public String[] getLaunchArguments() {
        return launchArguments.toArray(new String[0]);
    }
}
