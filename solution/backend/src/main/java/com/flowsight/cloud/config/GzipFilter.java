package com.flowsight.cloud.config;

import jakarta.servlet.*;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;

import java.io.IOException;

/**
 * GZIP 解压过滤器
 * 识别 HTTP Header [Content-Encoding: gzip] 并自动包装输入流，使 Spring Controller 透明无感接收 DTO
 */
@Component
@Order(-100)
public class GzipFilter implements Filter {

    @Override
    public void doFilter(ServletRequest servletRequest, ServletResponse servletResponse, FilterChain filterChain)
            throws IOException, ServletException {
        if (servletRequest instanceof HttpServletRequest httpRequest) {
            String contentEncoding = httpRequest.getHeader("Content-Encoding");
            if (contentEncoding != null && contentEncoding.toLowerCase().contains("gzip")) {
                filterChain.doFilter(new GzipHttpServletRequestWrapper(httpRequest), servletResponse);
                return;
            }
        }
        filterChain.doFilter(servletRequest, servletResponse);
    }
}
