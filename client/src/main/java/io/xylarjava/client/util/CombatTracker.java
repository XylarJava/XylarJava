package io.xylarjava.client.util;

import net.minecraft.client.Minecraft;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.util.AxisAlignedBB;
import net.minecraft.util.MovingObjectPosition;
import net.minecraft.util.Vec3;

/** Reach of the last hit and the current combo, fed by the attack hook. */
public final class CombatTracker {

    private static double lastReach;
    private static int combo;
    private static long lastHit;
    private static int lastHurtTime;

    private CombatTracker() {}

    public static void onAttack(EntityPlayer player, Entity target) {
        Vec3 eyes = player.getPositionEyes(1f);
        Vec3 look = player.getLook(1f);
        Vec3 end = eyes.addVector(look.xCoord * 6.0, look.yCoord * 6.0, look.zCoord * 6.0);
        float border = target.getCollisionBorderSize();
        AxisAlignedBB box = target.getEntityBoundingBox().expand(border, border, border);
        MovingObjectPosition hit = box.calculateIntercept(eyes, end);
        lastReach = hit != null ? eyes.distanceTo(hit.hitVec) : player.getDistanceToEntity(target);

        long now = System.currentTimeMillis();
        combo = now - lastHit > 3000L ? 1 : combo + 1;
        lastHit = now;
    }

    /** Called every tick: getting hit or waiting too long ends the combo. */
    public static void tick(Minecraft mc) {
        if (mc.thePlayer == null)
            return;
        int hurt = mc.thePlayer.hurtTime;
        if (hurt > 0 && lastHurtTime == 0)
            combo = 0;
        lastHurtTime = hurt;
        if (System.currentTimeMillis() - lastHit > 3000L)
            combo = 0;
    }

    public static double lastReach() {
        return lastReach;
    }

    public static int combo() {
        return combo;
    }
}
