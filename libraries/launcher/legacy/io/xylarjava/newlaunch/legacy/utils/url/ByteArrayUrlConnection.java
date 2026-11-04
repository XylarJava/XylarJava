package io.xylarjava.newlaunch.legacy.utils.url;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;

public final class ByteArrayUrlConnection extends HttpURLConnection {
    private final InputStream in;

    public ByteArrayUrlConnection(byte[] data) {
        super(null);
        this.in = new ByteArrayInputStream(data);
    }

    @Override
    public void connect() throws IOException {
        responseCode = 200;
    }

    @Override
    public void disconnect() {}

    @Override
    public InputStream getInputStream() throws IOException {
        return in;
    }

    @Override
    public boolean usingProxy() {
        return false;
    }
}
