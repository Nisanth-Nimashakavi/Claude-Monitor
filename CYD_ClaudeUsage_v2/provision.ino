// ═══════════════════════════════════════════════════════
//  provision.ino — first-time setup: Wi-Fi access point +
//  captive portal where you enter Wi-Fi, token, time zone.
// ═══════════════════════════════════════════════════════

static WebServer provWebServer(80);
static DNSServer provDnsServer;
static const byte DNS_PORT = 53;

// ── Build <option> tags from a WiFi scan (2.4 GHz only) ──
// The ESP32 radio can only join 2.4 GHz networks, but scans also
// list 5 GHz APs on dual-band routers. 2.4 GHz uses channels
// 1–14, 5 GHz uses 36 and up, so anything above 14 is hidden.
static String buildWifiOptions() {
    int n = WiFi.scanNetworks();
    if (n <= 0) return "<option disabled>No networks found</option>";

    String opts = "";
    int shown = 0;
    for (int i = 0; i < n; i++) {
        if (WiFi.channel(i) > 14) continue;      // 5 GHz
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;            // hidden network
        int  rssi = WiFi.RSSI(i);
        bool enc  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

        const char* sig = rssi > -55 ? "▂▄▆█" : rssi > -65 ? "▂▄▆" : rssi > -75 ? "▂▄" : "▂";

        String safe = ssid;
        safe.replace("&", "&amp;");
        safe.replace("\"", "&quot;");
        safe.replace("<", "&lt;");
        safe.replace(">", "&gt;");

        opts += "<option value=\"" + safe + "\">" + safe + " " + sig;
        if (enc) opts += " 🔒";
        opts += "</option>\n";
        shown++;
    }
    WiFi.scanDelete();
    if (shown == 0) return "<option disabled>No 2.4 GHz networks found</option>";
    return opts;
}

static void provHandleRoot() {
    String html = FPSTR(SETUP_HTML);
    html.replace("%CSS%", FPSTR(WEB_CSS));
    html.replace("%TZJS%", FPSTR(WEB_TZJS));
    html.replace("%WIFI_OPTIONS%", buildWifiOptions());
    provWebServer.send(200, "text/html", html);
}

static void provHandleScan() {
    provWebServer.send(200, "text/html", buildWifiOptions());
}

static void provHandleProvision() {
    String ssid      = provWebServer.arg("ssid");
    String wifipass  = provWebServer.arg("wifipass");
    String token_in  = provWebServer.arg("token");
    String pollStr   = provWebServer.arg("poll_sec");
    String brightStr = provWebServer.arg("brightness");
    String tz        = provWebServer.arg("tz");

    token_in.trim();
    ssid.trim();
    tz.trim();

    if (ssid.isEmpty() || token_in.isEmpty()) {
        provWebServer.send(400, "text/plain", "Missing Wi-Fi network or token.");
        return;
    }
    if (!token_in.startsWith("sk-ant-")) {
        provWebServer.send(400, "text/plain", "Token must start with sk-ant-");
        return;
    }

    EncryptedBlob blob;
    memset(&blob, 0, sizeof(blob));
    if (!encryptToken(token_in.c_str(), blob)) {
        provWebServer.send(500, "text/plain", "Couldn't store the token.");
        return;
    }

    int pollSec = pollStr.isEmpty() ? DEFAULT_POLL_SEC : constrain(pollStr.toInt(), MIN_POLL_SEC, MAX_POLL_SEC);
    int bright  = brightStr.isEmpty() ? DEFAULT_BRIGHTNESS : constrain(brightStr.toInt(), 0, 3);

    Preferences p;
    p.begin(NVS_NAMESPACE, false);
    p.putString("ssid",      ssid);
    p.putString("wifipass",  wifipass);
    p.putBytes("blob",       &blob, sizeof(blob));
    p.putInt("poll_sec",     pollSec);
    p.putInt("brightness",   bright);
    if (validTz(tz)) p.putString("tz", tz);
    p.putBool("provisioned", true);
    p.end();

    provWebServer.send(200, "text/plain", "OK");
    showBoot(100, "Saved", "Restarting...");
    delay(1500);
    ESP.restart();
}

static void provHandleNotFound() {
    provWebServer.sendHeader("Location", "http://192.168.4.1/", true);
    provWebServer.send(302, "text/plain", "");
}

void runProvisioningPortal(const char* apName, const char* apPass) {
    // Leave station mode cleanly, then start our own access point
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName, apPass);
    delay(100);

    provDnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    provWebServer.on("/",          HTTP_GET,  provHandleRoot);
    provWebServer.on("/scan",      HTTP_GET,  provHandleScan);
    provWebServer.on("/provision", HTTP_POST, provHandleProvision);
    provWebServer.onNotFound(provHandleNotFound);
    provWebServer.begin();

    ui.screen = SCR_SETUP;
    strlcpy(ui.apName, apName, sizeof(ui.apName));
    strlcpy(ui.apPass, apPass, sizeof(ui.apPass));
    displayRender();

    while (true) {
        provDnsServer.processNextRequest();
        provWebServer.handleClient();
        delay(2);
    }
}
