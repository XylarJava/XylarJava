package io.xylarjava.newlaunch.exception;

public final class ParseException extends IllegalArgumentException {
    private static final long serialVersionUID = 1L;

    public ParseException(String input, String format) {
        super(String.format("For input '%s' - should match '%s'", input, format));
    }
}
