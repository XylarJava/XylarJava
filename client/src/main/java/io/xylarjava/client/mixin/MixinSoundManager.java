package io.xylarjava.client.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import net.minecraft.client.audio.SoundManager;

/**
 * Vanilla 1.8.9 starts the sound system on a "Sound Library Loader" thread and only marks it loaded
 * once that thread is done. A second resource reload in the meantime (dragging the mipmap slider,
 * switching resource packs quickly) sees "not loaded", skips the shutdown and starts a second sound
 * system: "Only one OpenAL context may be instantiated", then an UnsatisfiedLinkError crash.
 * Wait for the pending start before shutting the sound system down.
 */
@Mixin(SoundManager.class)
public abstract class MixinSoundManager {
    private static final long XYLAR_LOADER_WAIT_MS = 10000;

    @Shadow private boolean loaded;

    private volatile boolean xylar$loaderPending;

    @Inject(method = "loadSoundSystem", at = @At("HEAD"))
    private void xylar$markLoaderPending(CallbackInfo ci) {
        if (!this.loaded)
            xylar$loaderPending = true;
    }

    @Inject(method = "unloadSoundSystem", at = @At("HEAD"))
    private void xylar$waitForLoader(CallbackInfo ci) {
        if (!xylar$loaderPending)
            return;
        // If the start failed, "loaded" never turns true: give up after a while, like vanilla.
        long deadline = System.currentTimeMillis() + XYLAR_LOADER_WAIT_MS;
        while (!this.loaded && System.currentTimeMillis() < deadline) {
            try {
                Thread.sleep(10);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                break;
            }
        }
        xylar$loaderPending = false;
    }
}
