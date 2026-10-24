package com.flowsight.cloud.config;

import jakarta.servlet.ReadListener;
import jakarta.servlet.ServletInputStream;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletRequestWrapper;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PushbackInputStream;
import java.nio.charset.StandardCharsets;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;

/**
 * GZIP / Deflate HTTP Request Wrapper
 * 智能自适应解压来自客户端携带 Content-Encoding: gzip 或 deflate 的二进制请求体输入流
 */
public class GzipHttpServletRequestWrapper extends HttpServletRequestWrapper {

    private final HttpServletRequest request;

    public GzipHttpServletRequestWrapper(HttpServletRequest request) {
        super(request);
        this.request = request;
    }

    @Override
    public ServletInputStream getInputStream() throws IOException {
        ServletInputStream rawStream = request.getInputStream();
        PushbackInputStream pushbackInputStream = new PushbackInputStream(rawStream, 4);

        byte[] header = new byte[2];
        int count = pushbackInputStream.read(header);
        if (count > 0) {
            pushbackInputStream.unread(header, 0, count);
        }

        final InputStream decompressStream;
        if (count >= 2 && header[0] == (byte) 0x1f && header[1] == (byte) 0x8b) {
            // 标准 GZIP 魔法字 (0x1f, 0x8b)
            decompressStream = new GZIPInputStream(pushbackInputStream);
        } else {
            // ZLIB / Deflate 压缩流
            decompressStream = new InflaterInputStream(pushbackInputStream);
        }

        return new ServletInputStream() {
            @Override
            public boolean isFinished() {
                try {
                    return decompressStream.available() == 0;
                } catch (IOException e) {
                    return true;
                }
            }

            @Override
            public boolean isReady() {
                return true;
            }

            @Override
            public void setReadListener(ReadListener readListener) {
            }

            @Override
            public int read() throws IOException {
                return decompressStream.read();
            }

            @Override
            public int read(byte[] b, int off, int len) throws IOException {
                return decompressStream.read(b, off, len);
            }

            @Override
            public void close() throws IOException {
                decompressStream.close();
            }
        };
    }

    @Override
    public BufferedReader getReader() throws IOException {
        return new BufferedReader(new InputStreamReader(this.getInputStream(), StandardCharsets.UTF_8));
    }
}
