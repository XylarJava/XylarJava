package io.xylarjava.newlaunch.legacy.utils;

import io.xylarjava.newlaunch.utils.logging.Log;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.nio.charset.StandardCharsets;

/**
 * Uses Base64 with Java 8 or later, otherwise DatatypeConverter. In the latter
 * case, reflection is used to allow using newer compilers.
 */
public final class Base64 {
    private static boolean supported = true;
    private static MethodHandle legacy;

    static {
        try {
            Class.forName("java.util.Base64");
        } catch (ClassNotFoundException e) {
            try {
                Class<?> datatypeConverter = Class.forName("javax.xml.bind.DatatypeConverter");
                legacy = MethodHandles.lookup().findStatic(
                        datatypeConverter, "parseBase64Binary", MethodType.methodType(byte[].class, String.class));
            } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException e1) {
                Log.error("Base64 not supported", e1);
                supported = false;
            }
        }
    }

    /**
     * Determines whether base64 is supported.
     *
     * @return <code>true</code> if base64 can be parsed
     */
    public static boolean isSupported() {
        return supported;
    }

    public static byte[] decode(String input) {
        if (!isSupported())
            throw new UnsupportedOperationException();

        if (legacy == null)
            return java.util.Base64.getDecoder().decode(input.getBytes(StandardCharsets.UTF_8));

        try {
            return (byte[]) legacy.invokeExact(input);
        } catch (Error | RuntimeException e) {
            throw e;
        } catch (Throwable e) {
            throw new Error(e);
        }
    }
}
