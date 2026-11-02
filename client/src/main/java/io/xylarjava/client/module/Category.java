package io.xylarjava.client.module;

/** Where a module shows up in the mod menu. */
public enum Category {
    PERFORMANCE("Performance"),
    HUD("HUD"),
    VISUAL("Visual"),
    CHAT("Chat"),
    PVP("PvP"),
    UTILITY("Utility");

    private final String title;

    Category(String title) {
        this.title = title;
    }

    public String title() {
        return title;
    }
}
