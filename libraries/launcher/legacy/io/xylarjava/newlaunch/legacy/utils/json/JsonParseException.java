package io.xylarjava.newlaunch.legacy.utils.json;

import java.io.IOException;

public final class JsonParseException extends IOException {
    private static final long serialVersionUID = 1L;

    public JsonParseException(String message) {
        super(message);
    }
}
