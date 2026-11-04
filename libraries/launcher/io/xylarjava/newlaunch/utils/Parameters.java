package io.xylarjava.newlaunch.utils;

import io.xylarjava.newlaunch.exception.ParameterNotFoundException;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class Parameters {
    private final Map<String, List<String>> map = new HashMap<>();

    public void add(String key, String value) {
        List<String> params = map.get(key);

        if (params == null) {
            params = new ArrayList<>();

            map.put(key, params);
        }

        params.add(value);
    }

    public List<String> getList(String key) throws ParameterNotFoundException {
        List<String> params = map.get(key);

        if (params == null)
            throw new ParameterNotFoundException(key);

        return params;
    }

    public List<String> getList(String key, List<String> def) {
        List<String> params = map.get(key);

        if (params == null || params.isEmpty())
            return def;

        return params;
    }

    public String getString(String key) throws ParameterNotFoundException {
        List<String> list = getList(key);

        if (list.isEmpty())
            throw new ParameterNotFoundException(key);

        return list.get(0);
    }

    public String getString(String key, String def) {
        List<String> params = map.get(key);

        if (params == null || params.isEmpty())
            return def;

        return params.get(0);
    }
}
