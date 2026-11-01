package io.xylarjava.client.util;

import java.util.ArrayDeque;
import java.util.Deque;

/** Counts clicks per second for the left and right mouse buttons. */
public final class ClickTracker {

    private static final Deque<Long> LEFT = new ArrayDeque<Long>();
    private static final Deque<Long> RIGHT = new ArrayDeque<Long>();

    private ClickTracker() {}

    public static void left() {
        LEFT.addLast(System.currentTimeMillis());
    }

    public static void right() {
        RIGHT.addLast(System.currentTimeMillis());
    }

    public static int leftCps() {
        return count(LEFT);
    }

    public static int rightCps() {
        return count(RIGHT);
    }

    private static int count(Deque<Long> clicks) {
        long cutoff = System.currentTimeMillis() - 1000L;
        while (!clicks.isEmpty() && clicks.peekFirst() < cutoff)
            clicks.removeFirst();
        return clicks.size();
    }
}
