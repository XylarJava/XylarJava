package io.xylarjava.client.mixin;

import java.util.List;

import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.xylarjava.client.Modules;
import net.minecraft.block.state.IBlockState;
import net.minecraft.client.particle.EffectRenderer;
import net.minecraft.client.particle.EntityFX;
import net.minecraft.util.BlockPos;
import net.minecraft.util.EnumFacing;

@Mixin(EffectRenderer.class)
public abstract class MixinEffectRenderer {
    @Shadow
    private List<EntityFX>[][] fxLayers;

    @Inject(method = "addEffect", at = @At("HEAD"), cancellable = true)
    private void xylar$particleLimit(EntityFX effect, CallbackInfo ci) {
        if (!Modules.PARTICLE_LIMIT.isEnabled())
            return;
        int total = 0;
        for (List<EntityFX>[] layer : this.fxLayers) {
            for (List<EntityFX> list : layer)
                total += list.size();
        }
        if (total >= Modules.PARTICLE_LIMIT.max())
            ci.cancel();
    }

    @Inject(method = "addBlockDestroyEffects", at = @At("HEAD"), cancellable = true)
    private void xylar$noBreakParticles(BlockPos pos, IBlockState state, CallbackInfo ci) {
        if (Modules.BLOCK_PARTICLES.isEnabled() && Modules.BLOCK_PARTICLES.breaking.on())
            ci.cancel();
    }

    @Inject(method = "addBlockHitEffects", at = @At("HEAD"), cancellable = true)
    private void xylar$noHitParticles(BlockPos pos, EnumFacing side, CallbackInfo ci) {
        if (Modules.BLOCK_PARTICLES.isEnabled() && Modules.BLOCK_PARTICLES.hitting.on())
            ci.cancel();
    }
}
