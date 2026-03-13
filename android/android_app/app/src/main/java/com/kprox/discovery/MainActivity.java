package com.kprox.discovery;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import androidx.appcompat.app.AppCompatActivity;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import org.json.JSONObject;

public class MainActivity extends AppCompatActivity {

    private static final int    UDP_DISCOVERY_PORT = 48269;
    private static final String DEFAULT_API_KEY    = "kprox1337";
    private static final String PREFS_NAME         = "KProxPrefs";
    private static final String PREF_API_KEY       = "apiKey";

    private WebView          webView;
    private ExecutorService  executor;
    private Handler          mainHandler;
    private NsdManager       nsdManager;
    private NsdManager.DiscoveryListener discoveryListener;
    private List<NsdServiceInfo> discoveredServices = new ArrayList<>();
    private AtomicBoolean    mdnsActive             = new AtomicBoolean(false);
    private AtomicBoolean    udpListenerActive      = new AtomicBoolean(false);
    private Thread           udpListenerThread;
    private DatagramSocket   udpSocket;
    private String           apiKey;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        executor    = Executors.newCachedThreadPool();
        mainHandler = new Handler(Looper.getMainLooper());
        nsdManager  = (NsdManager) getSystemService(Context.NSD_SERVICE);

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        apiKey = prefs.getString(PREF_API_KEY, DEFAULT_API_KEY);

        webView = new WebView(this);
        webView.getSettings().setJavaScriptEnabled(true);
        webView.getSettings().setDomStorageEnabled(true);
        webView.addJavascriptInterface(new AndroidInterface(), "AndroidApp");

        webView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                if (url.startsWith("http://") && url.contains("/api/")) return false;
                if (url.startsWith("http://")) {
                    startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
                    return true;
                }
                return false;
            }
        });

        setContentView(webView);
        webView.loadUrl("file:///android_asset/discovery.html");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopMdnsDiscovery();
        stopUdpDiscovery();
        if (executor != null) executor.shutdown();
    }

    // ---- HTTP helpers ----

    private String getNonce(String ip, int timeout) {
        try {
            URL url = new URL("http://" + ip + "/api/nonce");
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(timeout);
            conn.setReadTimeout(timeout);
            if (conn.getResponseCode() != 200) return null;
            return new JSONObject(readBody(conn)).getString("nonce");
        } catch (Exception e) {
            return null;
        }
    }

    private String readBody(HttpURLConnection conn) throws Exception {
        BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
        StringBuilder sb = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) sb.append(line);
        reader.close();
        conn.disconnect();
        return sb.toString();
    }

    private String decryptIfNeeded(HttpURLConnection conn, String body) throws Exception {
        if ("1".equals(conn.getHeaderField("X-Encrypted"))) {
            return KProxCrypto.decryptResponse(body, apiKey);
        }
        return body;
    }

    private String authenticatedGet(String ip, String path, int timeout) {
        try {
            String nonce = getNonce(ip, timeout);
            if (nonce == null) return "ERROR:Could not get nonce";

            URL url = new URL("http://" + ip + path);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(timeout);
            conn.setReadTimeout(timeout);
            conn.setRequestProperty("X-Auth", KProxCrypto.hmacAuth(nonce, apiKey));

            int code = conn.getResponseCode();
            if (code == 200) return "SUCCESS:" + decryptIfNeeded(conn, readBody(conn));
            return "HTTP_ERROR:" + code;
        } catch (Exception e) {
            return "ERROR:" + e.getMessage();
        }
    }

    private String authenticatedPost(String ip, String path, String jsonBody, int timeout) {
        try {
            String nonce     = getNonce(ip, timeout);
            if (nonce == null) return "ERROR:Could not get nonce";
            String encrypted = KProxCrypto.encryptBody(jsonBody, apiKey);

            URL url = new URL("http://" + ip + path);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("POST");
            conn.setConnectTimeout(timeout);
            conn.setReadTimeout(timeout);
            conn.setRequestProperty("Content-Type", "text/plain");
            conn.setRequestProperty("X-Auth",       KProxCrypto.hmacAuth(nonce, apiKey));
            conn.setRequestProperty("X-Encrypted",  "1");
            conn.setDoOutput(true);
            conn.getOutputStream().write(encrypted.getBytes(StandardCharsets.UTF_8));

            int code = conn.getResponseCode();
            if (code == 200) return "SUCCESS:" + decryptIfNeeded(conn, readBody(conn));
            return "HTTP_ERROR:" + code;
        } catch (Exception e) {
            return "ERROR:" + e.getMessage();
        }
    }

    private String authenticatedDelete(String ip, String path, int timeout) {
        try {
            String nonce = getNonce(ip, timeout);
            if (nonce == null) return "ERROR:Could not get nonce";

            URL url = new URL("http://" + ip + path);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("DELETE");
            conn.setConnectTimeout(timeout);
            conn.setReadTimeout(timeout);
            conn.setRequestProperty("X-Auth", KProxCrypto.hmacAuth(nonce, apiKey));

            int code = conn.getResponseCode();
            if (code == 200) return "SUCCESS:" + decryptIfNeeded(conn, readBody(conn));
            return "HTTP_ERROR:" + code;
        } catch (Exception e) {
            return "ERROR:" + e.getMessage();
        }
    }

    // ---- Device detection ----

    private String performDeviceTest(String ip) {
        String result = tryStatusEndpoint(ip);
        if (result.startsWith("SUCCESS:")) return result;
        return tryDiscoveryEndpoint(ip);
    }

    private String tryStatusEndpoint(String ip) {
        try {
            String result = authenticatedGet(ip, "/api/status", 2000);
            if (!result.startsWith("SUCCESS:")) return result;

            JSONObject json      = new JSONObject(result.substring(8));
            String bleDeviceName = json.optString("bleDeviceName", "").toLowerCase();
            String hostname      = json.optString("hostname",      "").toLowerCase();
            String boardType     = json.optString("boardType",     "");
            boolean hasUsbKb     = json.optBoolean("usbKeyboardReady", false);
            boolean hasUsbMouse  = json.optBoolean("usbMouseReady",    false);

            boolean isKProx = bleDeviceName.contains("kprox") ||
                              hostname.contains("kprox")       ||
                              boardType.contains("M5Stack")    ||
                              (hasUsbKb && hasUsbMouse);

            if (!isKProx) return "WRONG_TYPE:Not a KProx device";

            json.put("device_name", json.optString("hostname", "KProx Device"));
            json.put("device_id",   ip);
            json.put("ip",          ip);
            return "SUCCESS:" + json.toString();
        } catch (Exception e) {
            return "ERROR:" + e.getMessage();
        }
    }

    private String tryDiscoveryEndpoint(String ip) {
        try {
            String result = authenticatedGet(ip, "/api/discovery", 2000);
            if (!result.startsWith("SUCCESS:")) return result;

            JSONObject json   = new JSONObject(result.substring(8));
            String deviceType = json.optString("type", "").toLowerCase();
            if (!deviceType.equals("kprox") && !deviceType.equals("kprox_hid")) {
                return "WRONG_TYPE:" + deviceType;
            }
            json.put("device_name", json.optString("device_name", "KProx Device"));
            json.put("device_id",   ip);
            json.put("ip",          ip);
            return "SUCCESS:" + json.toString();
        } catch (Exception e) {
            return "ERROR:" + e.getMessage();
        }
    }

    // ---- mDNS ----

    private void stopMdnsDiscovery() {
        if (mdnsActive.getAndSet(false) && discoveryListener != null) {
            try { nsdManager.stopServiceDiscovery(discoveryListener); }
            catch (Exception e) { android.util.Log.e("KProxDebug", "mDNS stop error: " + e.getMessage()); }
        }
    }

    private void startMdnsDiscovery(String callbackId) {
        stopMdnsDiscovery();
        discoveredServices.clear();

        discoveryListener = new NsdManager.DiscoveryListener() {
            @Override public void onDiscoveryStarted(String t)    { mdnsActive.set(true); }
            @Override public void onDiscoveryStopped(String t)    { mdnsActive.set(false); }
            @Override public void onServiceLost(NsdServiceInfo s) {}
            @Override public void onStartDiscoveryFailed(String t, int e) {
                mdnsActive.set(false);
                stopMdnsDiscovery();
            }
            @Override public void onStopDiscoveryFailed(String t, int e) { mdnsActive.set(false); }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                nsdManager.resolveService(serviceInfo, new NsdManager.ResolveListener() {
                    @Override public void onResolveFailed(NsdServiceInfo s, int e) {}
                    @Override
                    public void onServiceResolved(NsdServiceInfo s) {
                        InetAddress host = s.getHost();
                        if (host == null) return;
                        String ip = host.getHostAddress();
                        if (ip == null) return;
                        discoveredServices.add(s);
                        executor.execute(() -> {
                            String result = performDeviceTest(ip);
                            mainHandler.post(() -> {
                                String[] parts = result.split(":", 2);
                                if ("SUCCESS".equals(parts[0])) {
                                    String data = parts.length > 1 ? parts[1] : "{}";
                                    webView.evaluateJavascript(
                                        "window._tempDeviceData = " + data + "; handleMdnsDeviceFound('" + callbackId + "', '" + ip + "');", null);
                                }
                            });
                        });
                    }
                });
            }
        };

        try {
            nsdManager.discoverServices("_http._tcp", NsdManager.PROTOCOL_DNS_SD, discoveryListener);
        } catch (Exception e) {
            android.util.Log.e("KProxDebug", "mDNS start failed: " + e.getMessage());
            mainHandler.post(() -> webView.evaluateJavascript("handleMdnsComplete('" + callbackId + "', false)", null));
        }
    }

    // ---- UDP Discovery ----

    private void startUdpDiscovery(String callbackId) {
        stopUdpDiscovery();
        udpListenerActive.set(true);

        udpListenerThread = new Thread(() -> {
            try {
                udpSocket = new DatagramSocket(UDP_DISCOVERY_PORT);
                udpSocket.setSoTimeout(1000);
                byte[] buf = new byte[8192];

                while (udpListenerActive.get()) {
                    try {
                        DatagramPacket packet = new DatagramPacket(buf, buf.length);
                        udpSocket.receive(packet);

                        String senderIp = packet.getAddress().getHostAddress();
                        String data     = new String(packet.getData(), 0, packet.getLength(), StandardCharsets.UTF_8).trim();

                        try {
                            String     decrypted = KProxCrypto.decryptResponse(data, apiKey);
                            JSONObject json       = new JSONObject(decrypted);

                            if (!"KProx-Discovery".equals(json.optString("protocol"))) continue;

                            String deviceIp = json.optString("ip", senderIp);
                            json.put("ip",          deviceIp);
                            json.put("device_id",   json.optString("device_id",   deviceIp));
                            json.put("device_name", json.optString("device_name", "KProx Device"));

                            final String finalJson = json.toString();
                            final String finalIp   = deviceIp;
                            mainHandler.post(() -> webView.evaluateJavascript(
                                "window._tempDeviceData = " + finalJson + "; handleUdpDeviceFound('" + callbackId + "', '" + finalIp + "');", null));

                        } catch (Exception e) {
                            android.util.Log.d("KProxDebug", "UDP decrypt failed from " + senderIp + ": " + e.getMessage());
                        }
                    } catch (java.net.SocketTimeoutException ignored) {}
                }
            } catch (Exception e) {
                android.util.Log.e("KProxDebug", "UDP listener error: " + e.getMessage());
            } finally {
                if (udpSocket != null && !udpSocket.isClosed()) { udpSocket.close(); udpSocket = null; }
            }
        });
        udpListenerThread.setDaemon(true);
        udpListenerThread.start();
    }

    private void stopUdpDiscovery() {
        udpListenerActive.set(false);
        if (udpSocket != null) { udpSocket.close(); udpSocket = null; }
        if (udpListenerThread != null) {
            try { udpListenerThread.join(2000); } catch (InterruptedException ignored) {}
            udpListenerThread = null;
        }
    }

    // ---- AndroidInterface (JS bridge) ----

    public class AndroidInterface {

        @JavascriptInterface
        public String getApiKey() { return apiKey; }

        @JavascriptInterface
        public void setApiKey(String key) {
            if (key == null || key.trim().isEmpty()) return;
            apiKey = key.trim();
            getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit().putString(PREF_API_KEY, apiKey).apply();
        }

        @JavascriptInterface
        public void startMdnsSearch(String callbackId) {
            mainHandler.post(() -> startMdnsDiscovery(callbackId));
        }

        @JavascriptInterface
        public void stopMdnsSearch() {
            mainHandler.post(() -> stopMdnsDiscovery());
        }

        @JavascriptInterface
        public void startUdpDiscovery(String callbackId) {
            mainHandler.post(() -> MainActivity.this.startUdpDiscovery(callbackId));
        }

        @JavascriptInterface
        public void stopUdpDiscovery() {
            executor.execute(() -> MainActivity.this.stopUdpDiscovery());
        }

        @JavascriptInterface
        public void scanAllNetworks(String callbackId) {
            executor.execute(() -> {
                AtomicInteger totalScanned = new AtomicInteger(0);
                AtomicInteger devicesFound = new AtomicInteger(0);
                for (int second = 0; second < 256; second++) {
                    final int so = second;
                    for (int third = 0; third < 256; third++) {
                        final int to = third;
                        executor.execute(() -> {
                            for (int fourth = 1; fourth < 255; fourth++) {
                                String ip = "10." + so + "." + to + "." + fourth;
                                try {
                                    if (InetAddress.getByName(ip).isReachable(100)) {
                                        String result = performDeviceTest(ip);
                                        if (result.startsWith("SUCCESS:")) {
                                            devicesFound.incrementAndGet();
                                            String[] parts = result.split(":", 2);
                                            String data = parts.length > 1 ? parts[1] : "{}";
                                            mainHandler.post(() -> webView.evaluateJavascript(
                                                "window._tempDeviceData = " + data + "; handleScanDeviceFound('" + callbackId + "', '" + ip + "');", null));
                                        }
                                    }
                                } catch (Exception ignored) {}
                                int scanned = totalScanned.incrementAndGet();
                                if (scanned % 1000 == 0) {
                                    final int s = scanned, f = devicesFound.get();
                                    mainHandler.post(() -> webView.evaluateJavascript(
                                        "handleScanProgress('" + callbackId + "', " + s + ", " + f + ")", null));
                                }
                            }
                        });
                    }
                }
                mainHandler.postDelayed(() -> webView.evaluateJavascript("handleScanComplete('" + callbackId + "')", null), 30000);
            });
        }

        @JavascriptInterface
        public String getLocalIP() {
            try {
                for (NetworkInterface intf : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                    if (!intf.isUp() || intf.isLoopback()) continue;
                    for (InetAddress addr : Collections.list(intf.getInetAddresses())) {
                        if (!addr.isLoopbackAddress()) {
                            String ip = addr.getHostAddress();
                            if (ip != null && ip.indexOf(':') < 0 && ip.startsWith("10.")) return ip;
                        }
                    }
                }
            } catch (Exception e) { e.printStackTrace(); }
            return null;
        }

        @JavascriptInterface
        public String getAllNetworkInterfaces() {
            try {
                StringBuilder result = new StringBuilder();
                for (NetworkInterface intf : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                    if (!intf.isUp() || intf.isLoopback()) continue;
                    for (InetAddress addr : Collections.list(intf.getInetAddresses())) {
                        if (!addr.isLoopbackAddress()) {
                            String ip = addr.getHostAddress();
                            if (ip != null && ip.indexOf(':') < 0 && ip.startsWith("10.")) {
                                if (result.length() > 0) result.append(",");
                                result.append(ip);
                            }
                        }
                    }
                }
                return result.toString();
            } catch (Exception e) {
                android.util.Log.e("KProxDebug", "getAllNetworkInterfaces: " + e.getMessage());
                return "";
            }
        }

        @JavascriptInterface
        public String getClipboardText() {
            try {
                android.content.ClipboardManager cb = (android.content.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
                if (cb != null && cb.hasPrimaryClip()) {
                    android.content.ClipData.Item item = cb.getPrimaryClip().getItemAt(0);
                    if (item != null && item.getText() != null) return item.getText().toString();
                }
            } catch (Exception e) { android.util.Log.e("KProxDebug", "Clipboard: " + e.getMessage()); }
            return null;
        }

        @JavascriptInterface
        public void testDevice(String ip, String callbackId) {
            executor.execute(() -> {
                String result = performDeviceTest(ip);
                mainHandler.post(() -> {
                    String[] parts = result.split(":", 2);
                    if ("SUCCESS".equals(parts[0])) {
                        String data = parts.length > 1 ? parts[1] : "{}";
                        webView.evaluateJavascript(
                            "window._tempDeviceData = " + data + "; handleDeviceTestResult('" + callbackId + "', 'SUCCESS_STORED');", null);
                    } else {
                        String escaped = result.replace("'", "\\'");
                        webView.evaluateJavascript("handleDeviceTestResult('" + callbackId + "', '" + escaped + "')", null);
                    }
                });
            });
        }

        @JavascriptInterface
        public void openUrl(String url) {
            mainHandler.post(() -> startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url))));
        }

        @JavascriptInterface
        public void sendText(String deviceIp, String text, String callbackId) {
            executor.execute(() -> {
                String result = performTextSend(deviceIp, text);
                mainHandler.post(() -> {
                    String escaped = result.replace("'", "\\'");
                    webView.evaluateJavascript("handleTextSendResult('" + callbackId + "', '" + escaped + "')", null);
                });
            });
        }

        private String performTextSend(String deviceIp, String text) {
            try {
                text = text.replaceAll("[^\\x00-\\x7F]", "");
                JSONObject body = new JSONObject();
                body.put("text", text);
                return authenticatedPost(deviceIp, "/send/text", body.toString(), 5000);
            } catch (Exception e) {
                return "ERROR:" + e.getMessage();
            }
        }

        @JavascriptInterface
        public void loadRegisters(String deviceIp, String callbackId) {
            executor.execute(() -> {
                String result = authenticatedGet(deviceIp, "/api/registers", 5000);
                mainHandler.post(() -> {
                    String[] parts = result.split(":", 2);
                    if ("SUCCESS".equals(parts[0])) {
                        String data = parts.length > 1 ? parts[1] : "{}";
                        webView.evaluateJavascript(
                            "window._tempRegisterData = " + data + "; handleLoadRegistersResult('" + callbackId + "', 'SUCCESS:' + JSON.stringify(window._tempRegisterData));", null);
                    } else {
                        String escaped = result.replace("'", "\\'");
                        webView.evaluateJavascript("handleLoadRegistersResult('" + callbackId + "', '" + escaped + "')", null);
                    }
                });
            });
        }

        @JavascriptInterface
        public void saveRegisters(String deviceIp, String registersJson, String callbackId) {
            executor.execute(() -> {
                String result = authenticatedPost(deviceIp, "/api/registers", registersJson, 5000);
                mainHandler.post(() -> {
                    String escaped = result.replace("'", "\\'");
                    webView.evaluateJavascript("handleSaveRegistersResult('" + callbackId + "', '" + escaped + "')", null);
                });
            });
        }

        @JavascriptInterface
        public void deleteAllRegisters(String deviceIp, String callbackId) {
            executor.execute(() -> {
                String result = authenticatedDelete(deviceIp, "/api/registers", 5000);
                mainHandler.post(() -> {
                    String escaped = result.replace("'", "\\'");
                    webView.evaluateJavascript("handleDeleteAllRegistersResult('" + callbackId + "', '" + escaped + "')", null);
                });
            });
        }

        @JavascriptInterface
        public void saveTabsToFile(String json, String filename) {
            try {
                java.io.File dir  = android.os.Environment.getExternalStoragePublicDirectory(android.os.Environment.DIRECTORY_DOWNLOADS);
                java.io.File file = new java.io.File(dir, filename);
                java.io.FileWriter writer = new java.io.FileWriter(file);
                writer.write(json);
                writer.close();
                final String path = file.getAbsolutePath();
                mainHandler.post(() -> webView.evaluateJavascript("handleFileSaveResult('success', '" + path + "')", null));
            } catch (Exception e) {
                final String err = e.getMessage().replace("'", "\\'");
                mainHandler.post(() -> webView.evaluateJavascript("handleFileSaveResult('error', '" + err + "')", null));
            }
        }

        @JavascriptInterface
        public void loadTabsFromFile(String filename) {
            try {
                java.io.File dir  = android.os.Environment.getExternalStoragePublicDirectory(android.os.Environment.DIRECTORY_DOWNLOADS);
                java.io.File file = new java.io.File(dir, filename);
                java.io.BufferedReader reader = new java.io.BufferedReader(new java.io.FileReader(file));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) sb.append(line);
                reader.close();
                final String jsonStr = sb.toString();
                mainHandler.post(() -> webView.evaluateJavascript(
                    "window._tempLoadedData = " + jsonStr + "; handleFileLoadResult('success');", null));
            } catch (Exception e) {
                final String err = e.getMessage().replace("'", "\\'");
                mainHandler.post(() -> webView.evaluateJavascript("handleFileLoadResult('error', '" + err + "')", null));
            }
        }
    }
}
