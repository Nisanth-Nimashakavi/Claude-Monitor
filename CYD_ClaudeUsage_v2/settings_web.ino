// ═══════════════════════════════════════════════════════
//  settings_web.ino — settings page on your home network
//    http://claude-monitor.local  (or the IP on the Device page)
//
//  Only display preferences can be changed here. The Claude
//  token is never sent out by this page, and Wi-Fi/token can
//  only be changed with the BOOT-button factory reset.
// ═══════════════════════════════════════════════════════

static WebServer webServer(80);
static bool      webStarted = false;

static String pageWithAssets(const char* page) {
    String html = FPSTR(page);
    html.replace("%CSS%", FPSTR(WEB_CSS));
    html.replace("%TZJS%", FPSTR(WEB_TZJS));
    return html;
}

static void jsonEscapeInto(String& out, const char* s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { out += '\\'; out += *s; }
        else if ((uint8_t)*s >= 0x20) out += *s;
    }
}

static void webHandleRoot() {
    webServer.send(200, "text/html", pageWithAssets(SETTINGS_HTML));
}

static void webHandleState() {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"v\":\"%s\",\"hasData\":%d,\"h5\":%.1f,\"d7\":%.1f,\"h5r\":%lu,\"d7r\":%lu,"
             "\"limited\":%d,\"now\":%lu,\"last\":%lu,\"rssi\":%d,\"net\":%d,"
             "\"clock24\":%d,\"brightness\":%d,\"poll_sec\":%d,\"night_on\":%d,\"night_from\":%d,"
             "\"night_to\":%d,\"night_lvl\":%d,\"start_page\":%d,\"rotate_sec\":%d,",
             FW_VERSION, ui.hasData, ui.h5, ui.d7, (unsigned long)ui.h5Reset, (unsigned long)ui.d7Reset,
             ui.limited, (unsigned long)(timeValid() ? time(nullptr) : 0), (unsigned long)ui.lastOkEpoch,
             WiFi.RSSI(), (int)ui.net, cfg.clock24, cfg.brightness, cfg.pollSec, cfg.nightOn,
             cfg.nightFrom, cfg.nightTo, cfg.nightLevel, cfg.startPage, cfg.rotateSec);
    String json = buf;
    json += "\"tz\":\"";  jsonEscapeInto(json, cfg.tz);        json += "\",";
    json += "\"err\":\""; jsonEscapeInto(json, ui.netDetail);  json += "\"}";
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "application/json", json);
}

static int argInt(const char* name, int def, int lo, int hi) {
    if (!webServer.hasArg(name)) return def;
    return constrain(webServer.arg(name).toInt(), lo, hi);
}

static void webHandleSave() {
    if (webServer.hasArg("tz")) {
        String tz = webServer.arg("tz");
        tz.trim();
        if (!validTz(tz)) { webServer.send(400, "text/plain", "That time zone isn't valid."); return; }
        strlcpy(cfg.tz, tz.c_str(), sizeof(cfg.tz));
    }
    cfg.clock24    = argInt("clock24", cfg.clock24, 0, 1);
    cfg.brightness = argInt("brightness", cfg.brightness, 0, 3);
    cfg.nightOn    = argInt("night_on", cfg.nightOn, 0, 1);
    cfg.nightFrom  = argInt("night_from", cfg.nightFrom, 0, 23);
    cfg.nightTo    = argInt("night_to", cfg.nightTo, 0, 23);
    cfg.nightLevel = argInt("night_lvl", cfg.nightLevel, 0, 1);
    cfg.startPage  = argInt("start_page", cfg.startPage, 0, PAGE_COUNT - 1);
    cfg.rotateSec  = argInt("rotate_sec", cfg.rotateSec, 0, 300);
    cfg.pollSec    = argInt("poll_sec", cfg.pollSec, MIN_POLL_SEC, MAX_POLL_SEC);

    saveSettings();
    applyTimezone();
    syncUiFromSettings();
    Serial.printf("[WEB] settings saved (tz=%s)\n", cfg.tz);
    webServer.send(200, "text/plain", "OK");
}

static void webHandleRefresh() {
    requestRefresh();
    webServer.send(200, "text/plain", "OK");
}

void webBegin() {
    if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
    webServer.on("/",             HTTP_GET,  webHandleRoot);
    webServer.on("/api/state",    HTTP_GET,  webHandleState);
    webServer.on("/api/settings", HTTP_POST, webHandleSave);
    webServer.on("/api/refresh",  HTTP_POST, webHandleRefresh);
    webServer.onNotFound([]() { webServer.send(404, "text/plain", "Not found"); });
    webServer.begin();
    webStarted = true;
    Serial.printf("[WEB] settings at http://%s.local\n", HOSTNAME);
}

void webLoop() {
    if (webStarted) webServer.handleClient();
}
