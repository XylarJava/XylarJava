package io.xylarjava.newlaunch.utils;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

public final class ReflectionUtils {
    private static final MethodHandles.Lookup LOOKUP = MethodHandles.lookup();
    private static final ClassLoader LOADER = ClassLoader.getSystemClassLoader();

    /**
     * Gets the main method within a class.
     *
     * @param clazz The class
     * @return A method matching the descriptor of a main method
     * @throws ClassNotFoundException
     * @throws NoSuchMethodException
     * @throws IllegalAccessException
     */
    public static MethodHandle findMainMethod(Class<?> clazz) throws NoSuchMethodException, IllegalAccessException {
        return LOOKUP.findStatic(clazz, "main", MethodType.methodType(void.class, String[].class));
    }

    /**
     * Gets the main method within a class by its name.
     *
     * @param clazz The class name
     * @return A method matching the descriptor of a main method
     * @throws ClassNotFoundException
     * @throws NoSuchMethodException
     * @throws IllegalAccessException
     */
    public static MethodHandle findMainMethod(String clazz) throws ClassNotFoundException, NoSuchMethodException, IllegalAccessException {
        return findMainMethod(LOADER.loadClass(clazz));
    }
}
