package com.kprox.discovery;

import android.util.Base64;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.Mac;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

// Mirrors the firmware AES-256-CTR + HMAC-SHA256 scheme in crypto_utils.cpp.
// Payload format: base64( iv[16] + ciphertext[n] + hmac[32] )
// AES key: SHA-256(apiKey UTF-8 bytes)
// CTR counter: iv with byte[15] forced to (byte[15] & 0xfe) | 0x01
public class KProxCrypto {

    private static byte[] sha256(byte[] data) throws Exception {
        return MessageDigest.getInstance("SHA-256").digest(data);
    }

    private static byte[] deriveKey(String apiKey) throws Exception {
        return sha256(apiKey.getBytes(StandardCharsets.UTF_8));
    }

    private static byte[] hmacSha256(byte[] key, byte[] iv, byte[] ciphertext) throws Exception {
        Mac mac = Mac.getInstance("HmacSHA256");
        mac.init(new SecretKeySpec(key, "HmacSHA256"));
        mac.update(iv);
        mac.update(ciphertext);
        return mac.doFinal();
    }

    private static byte[] ctrIvFrom(byte[] iv) {
        byte[] ctrIv = Arrays.copyOf(iv, 16);
        ctrIv[15] = (byte) ((ctrIv[15] & 0xfe) | 0x01);
        return ctrIv;
    }

    public static String decryptResponse(String base64, String apiKey) throws Exception {
        byte[] raw = Base64.decode(base64.trim(), Base64.DEFAULT);
        if (raw.length < 48) throw new Exception("Payload too short: " + raw.length);

        byte[] iv         = Arrays.copyOfRange(raw, 0, 16);
        byte[] ciphertext = Arrays.copyOfRange(raw, 16, raw.length - 32);
        byte[] tag        = Arrays.copyOfRange(raw, raw.length - 32, raw.length);
        byte[] key        = deriveKey(apiKey);

        byte[] expected = hmacSha256(key, iv, ciphertext);
        byte diff = 0;
        for (int i = 0; i < 32; i++) diff |= expected[i] ^ tag[i];
        if (diff != 0) throw new Exception("HMAC verification failed — wrong API key?");

        Cipher cipher = Cipher.getInstance("AES/CTR/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE,
                new SecretKeySpec(key, "AES"),
                new IvParameterSpec(ctrIvFrom(iv)));
        return new String(cipher.doFinal(ciphertext), StandardCharsets.UTF_8);
    }

    public static String encryptBody(String json, String apiKey) throws Exception {
        byte[] key        = deriveKey(apiKey);
        byte[] iv         = new byte[16];
        new SecureRandom().nextBytes(iv);
        byte[] plainBytes = json.getBytes(StandardCharsets.UTF_8);

        Cipher cipher = Cipher.getInstance("AES/CTR/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE,
                new SecretKeySpec(key, "AES"),
                new IvParameterSpec(ctrIvFrom(iv)));
        byte[] ciphertext = cipher.doFinal(plainBytes);

        byte[] tag  = hmacSha256(key, iv, ciphertext);
        byte[] blob = new byte[16 + ciphertext.length + 32];
        System.arraycopy(iv,         0, blob, 0,                    16);
        System.arraycopy(ciphertext, 0, blob, 16,                   ciphertext.length);
        System.arraycopy(tag,        0, blob, 16 + ciphertext.length, 32);

        return Base64.encodeToString(blob, Base64.NO_WRAP);
    }

    // Returns HMAC-SHA256(apiKey, nonce) as lowercase hex — used as X-Auth header value.
    public static String hmacAuth(String nonce, String apiKey) throws Exception {
        Mac mac = Mac.getInstance("HmacSHA256");
        mac.init(new SecretKeySpec(apiKey.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
        byte[] result = mac.doFinal(nonce.getBytes(StandardCharsets.UTF_8));
        StringBuilder hex = new StringBuilder(64);
        for (byte b : result) hex.append(String.format("%02x", b));
        return hex.toString();
    }
}
