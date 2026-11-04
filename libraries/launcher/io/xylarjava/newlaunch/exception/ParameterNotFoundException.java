package io.xylarjava.newlaunch.exception;

public final class ParameterNotFoundException extends IllegalArgumentException {
    private static final long serialVersionUID = 1L;

    public ParameterNotFoundException(String key) {
        super(String.format("Required parameter '%s' was not found", key));
    }
}
