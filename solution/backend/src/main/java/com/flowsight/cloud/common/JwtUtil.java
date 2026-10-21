package com.flowsight.cloud.common;

import io.jsonwebtoken.Claims;
import io.jsonwebtoken.Jwts;
import io.jsonwebtoken.SignatureAlgorithm;
import io.jsonwebtoken.security.Keys;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import javax.crypto.SecretKey;
import java.nio.charset.StandardCharsets;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;

@Component
public class JwtUtil {

    @Value("${jwt.secret}")
    private String secret;

    @Value("${jwt.expire-hours}")
    private int expireHours;

    private SecretKey getSigningKey() {
        byte[] keyBytes = secret.getBytes(StandardCharsets.UTF_8);
        if (keyBytes.length < 32) {
            byte[] padded = new byte[32];
            System.arraycopy(keyBytes, 0, padded, 0, keyBytes.length);
            keyBytes = padded;
        }
        return Keys.hmacShaKeyFor(keyBytes);
    }

    public String generateToken(Long userId, String phone, String role) {
        Map<String, Object> claims = new HashMap<>();
        claims.put("userId", userId);
        claims.put("phone", phone);
        claims.put("role", role);

        long now = System.currentTimeMillis();
        long expireTime = now + (long) expireHours * 3600 * 1000;

        return Jwts.builder()
                .setClaims(claims)
                .setSubject(phone)
                .setIssuedAt(new Date(now))
                .setExpiration(new Date(expireTime))
                .signWith(getSigningKey(), SignatureAlgorithm.HS256)
                .compact();
    }

    public Claims parseToken(String token) {
        try {
            return Jwts.parserBuilder()
                    .setSigningKey(getSigningKey())
                    .build()
                    .parseClaimsJws(token)
                    .getBody();
        } catch (Exception e) {
            return null;
        }
    }

    public boolean validateToken(String token) {
        Claims claims = parseToken(token);
        if (claims == null) return false;
        return !claims.getExpiration().before(new Date());
    }

    public Long getUserId(String token) {
        Claims claims = parseToken(token);
        if (claims == null) return null;
        Object val = claims.get("userId");
        if (val instanceof Number) {
            return ((Number) val).longValue();
        }
        return null;
    }

    public String getRole(String token) {
        Claims claims = parseToken(token);
        if (claims == null) return null;
        return (String) claims.get("role");
    }
}
