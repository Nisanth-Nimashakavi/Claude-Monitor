/*
 * Claude Usage Monitor v2 — CYD (ESP32-2432S028R)
 * ================================================
 * Shows your Claude 5-hour and weekly usage on the "Cheap
 * Yellow Display", polling api.anthropic.com directly with
 * your OAuth token.
 *
 * What's new in v2:
 *   - Large, anti-aliased text and gauges (DM Sans typeface)
 *   - Four pages: Overview · 5-hour · Weekly · Device (tap screen)
 *   - Clock, local reset times, and "on pace / runs out in" projection
 *   - Settings page on your network: http://claude-monitor.local
 *   - Night dimming schedule, auto-cycling pages
 *
 * LIBRARIES (Sketch > Include Library > Manage Libraries):
 *   - TFT_eSPI by Bodmer   (copy User_Setup.h into its folder)
 *
 * BOARD: ESP32 Dev Module, Partition Scheme "Huge APP (3MB No OTA)"
 *
 * CONTROLS:
 *   Tap screen         — next page (wakes the screen if it's off)
 *   Press & hold 1 s   — refresh now
 *   BOOT button tap    — cycle brightness (off · dim · normal · bright)
 *   BOOT hold 5 s      — factory reset (back to Wi-Fi/token setup)
 *
 * Upgrading from v1 keeps your saved Wi-Fi and token.
 */

// ── Includes (FS must come before WebServer) ──────────
#include <FS.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "screens.h"
#include "web_assets.h"

#define FW_VERSION "2.0"

// ── Config ────────────────────────────────────────────
#define DEFAULT_POLL_SEC       120
#define MIN_POLL_SEC            30
#define MAX_POLL_SEC           600
#define DEFAULT_BRIGHTNESS       2
#define SCREEN_ROT               1
#define WIFI_CONNECT_TIMEOUT_S  20
#define API_TIMEOUT_MS       15000
#define MESSAGES_ENDPOINT    "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION    "2023-06-01"
#define PROBE_MODEL          "claude-haiku-4-5-20251001"
#define NVS_NAMESPACE        "claude"
#define HOSTNAME             "claude-monitor"

// ── Pins (CYD) ────────────────────────────────────────
#define BTN_BOOT   0     // BOOT button, active LOW
#define BL_PIN    21     // backlight PWM
#define T_CLK     25     // XPT2046 touch controller (own pins,
#define T_MOSI    32     //  bit-banged so it can't clash with
#define T_MISO    39     //  the display's SPI bus)
#define T_CS      33

// ── Shared structs ────────────────────────────────────
struct UsageData {
    float    h5;
    float    d7;
    uint32_t h5ResetEpoch;
    uint32_t d7ResetEpoch;
    bool     ok;
    bool     limited;
    int      httpCode;
    char     error[64];
};

// Must stay byte-identical to v1 so a saved token keeps working
struct EncryptedBlob {
    uint8_t  salt[6];
    uint16_t len;
    uint8_t  ciphertext[512];
};

struct Settings {
    int      pollSec    = DEFAULT_POLL_SEC;
    uint8_t  brightness = DEFAULT_BRIGHTNESS;   // daytime level 0-3
    char     tz[64]     = "";                   // POSIX TZ string, "" = not chosen
    bool     clock24    = false;
    bool     nightOn    = false;
    uint8_t  nightFrom  = 22;                   // local hour dimming starts
    uint8_t  nightTo    = 7;                    // local hour it ends
    uint8_t  nightLevel = 1;                    // 0 = off, 1 = dim
    uint8_t  startPage  = PAGE_OVERVIEW;
    uint16_t rotateSec  = 0;                    // auto-cycle pages, 0 = off
};

// ── Globals ───────────────────────────────────────────
TFT_eSPI lcd;
Model    ui;
Settings cfg;

static Preferences   prefs;
static char          token[256];
static UsageData     usage;
static unsigned long lastFetchMs   = 0;
static unsigned long nextFetchInMs = 0;
static int           failStreak    = 0;
static bool          needRender    = true;
static bool          refreshWanted = false;

static uint8_t       curLevel        = DEFAULT_BRIGHTNESS;  // brightness in effect
static uint8_t       appliedLevel    = 255;
static bool          inNight         = false;
static unsigned long wakeUntil       = 0;     // screen temporarily on while "off"
static unsigned long toastUntil      = 0;
static unsigned long saveBrightAt    = 0;     // deferred NVS write
static unsigned long lastInteraction = 0;
static unsigned long lastRotate      = 0;

// ── Backlight ─────────────────────────────────────────
static void setBacklightRaw(uint8_t level) {
    static const uint8_t vals[] = {0, 60, 160, 255};
    ledcWrite(BL_PIN, vals[level & 3]);
}

static void applyBacklight() {
    uint8_t lvl = curLevel;
    if (lvl == 0 && millis() < wakeUntil) lvl = 2;
    if (ui.holdSecondsLeft >= 0 && lvl < 2) lvl = 2;
    if (lvl != appliedLevel) { setBacklightRaw(lvl); appliedLevel = lvl; }
}

// ── Settings in NVS ───────────────────────────────────
static void loadSettings() {
    prefs.begin(NVS_NAMESPACE, true);
    cfg.pollSec    = constrain(prefs.getInt("poll_sec", DEFAULT_POLL_SEC), MIN_POLL_SEC, MAX_POLL_SEC);
    cfg.brightness = constrain(prefs.getInt("brightness", DEFAULT_BRIGHTNESS), 0, 3);
    String tz      = prefs.getString("tz", "");
    strlcpy(cfg.tz, tz.c_str(), sizeof(cfg.tz));
    cfg.clock24    = prefs.getBool("clock24", false);
    cfg.nightOn    = prefs.getBool("night_on", false);
    cfg.nightFrom  = constrain(prefs.getInt("night_from", 22), 0, 23);
    cfg.nightTo    = constrain(prefs.getInt("night_to", 7), 0, 23);
    cfg.nightLevel = constrain(prefs.getInt("night_lvl", 1), 0, 1);
    cfg.startPage  = constrain(prefs.getInt("start_page", PAGE_OVERVIEW), 0, PAGE_COUNT - 1);
    cfg.rotateSec  = constrain(prefs.getInt("rotate_sec", 0), 0, 300);
    prefs.end();
}

void saveSettings() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt("poll_sec", cfg.pollSec);
    prefs.putInt("brightness", cfg.brightness);
    prefs.putString("tz", cfg.tz);
    prefs.putBool("clock24", cfg.clock24);
    prefs.putBool("night_on", cfg.nightOn);
    prefs.putInt("night_from", cfg.nightFrom);
    prefs.putInt("night_to", cfg.nightTo);
    prefs.putInt("night_lvl", cfg.nightLevel);
    prefs.putInt("start_page", cfg.startPage);
    prefs.putInt("rotate_sec", cfg.rotateSec);
    prefs.end();
}

// POSIX TZ strings only use these characters
bool validTz(const String& tz) {
    if (tz.length() == 0 || tz.length() >= sizeof(cfg.tz)) return false;
    for (size_t i = 0; i < tz.length(); i++) {
        char ch = tz[i];
        if (!(isalnum((unsigned char)ch) || strchr("<>+-:,./", ch))) return false;
    }
    return true;
}

void applyTimezone() {
    setenv("TZ", cfg.tz[0] ? cfg.tz : "UTC0", 1);
    tzset();
}

// Copy settings that the screens need into the UI model
void syncUiFromSettings() {
    ui.tzSet   = cfg.tz[0] != 0;
    ui.clock24 = cfg.clock24;
    ui.pollSec = cfg.pollSec;
    ui.version = FW_VERSION;
    strlcpy(ui.host, HOSTNAME, sizeof(ui.host));
    if (!inNight) curLevel = cfg.brightness;
    needRender = true;
}

static bool timeValid() { return time(nullptr) > 1700000000; }

// ── Touch (XPT2046, bit-banged) ───────────────────────
static void touchInit() {
    pinMode(T_CLK, OUTPUT);  digitalWrite(T_CLK, LOW);
    pinMode(T_MOSI, OUTPUT); digitalWrite(T_MOSI, LOW);
    pinMode(T_CS, OUTPUT);   digitalWrite(T_CS, HIGH);
    pinMode(T_MISO, INPUT);
}

static uint16_t xptTransfer(uint8_t cmd) {
    for (int i = 7; i >= 0; i--) {
        digitalWrite(T_MOSI, (cmd >> i) & 1);
        digitalWrite(T_CLK, HIGH); delayMicroseconds(1);
        digitalWrite(T_CLK, LOW);  delayMicroseconds(1);
    }
    digitalWrite(T_MOSI, LOW);
    uint16_t v = 0;
    for (int i = 0; i < 16; i++) {
        digitalWrite(T_CLK, HIGH); delayMicroseconds(1);
        v = (v << 1) | digitalRead(T_MISO);
        digitalWrite(T_CLK, LOW);  delayMicroseconds(1);
    }
    return v >> 3;   // 12-bit result
}

// Touch pressure: ~0 when untouched, several hundred+ when pressed
static int touchPressure() {
    digitalWrite(T_CS, LOW);
    int z1 = xptTransfer(0xB1);
    int z2 = xptTransfer(0xC1);
    xptTransfer(0xD0);           // power down between readings
    digitalWrite(T_CS, HIGH);
    // A missing/unresponsive chip reads all-0s or all-1s: treat as untouched
    if ((z1 == 0 && z2 == 0) || z1 > 4095 || z2 > 4095) return 0;
    return z1 + 4095 - z2;
}

static void onTap() {
    lastInteraction = millis();
    if (appliedLevel == 0) {               // screen is dark: just wake it
        wakeUntil = millis() + 30000;
        return;
    }
    if (curLevel == 0) wakeUntil = millis() + 30000;   // keep it lit while browsing
    if (ui.screen != SCR_DASH) return;
    ui.page = (ui.page + 1) % PAGE_COUNT;
    needRender = true;
}

static void onLongPress() {
    lastInteraction = millis();
    if (ui.screen == SCR_DASH) refreshWanted = true;
}

static void handleTouch() {
    static unsigned long lastPoll = 0;
    static uint8_t onCount = 0, offCount = 0;
    static bool down = false, longFired = false;
    static unsigned long downAt = 0;

    if (millis() - lastPoll < 15) return;
    lastPoll = millis();

    bool pressed = touchPressure() > 400;
    if (pressed) {
        offCount = 0;
        if (!down && ++onCount >= 2) { down = true; downAt = millis(); longFired = false; }
    } else {
        onCount = 0;
        if (down && ++offCount >= 3) {
            down = false;
            if (!longFired) onTap();
        }
    }
    if (down && !longFired && millis() - downAt > 1000) { longFired = true; onLongPress(); }
}

// ── BOOT button: tap = brightness, hold 5 s = reset ───
static void factoryReset() {
    ui.holdSecondsLeft = -1;
    showMessage("Resetting", "Clearing Wi-Fi and token. Setup starts next.", "", false);
    setBacklightRaw(2);
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    delay(1500);
    ESP.restart();
}

static void cycleBrightness() {
    curLevel = (curLevel + 1) % 4;
    wakeUntil = 0;
    if (!inNight) { cfg.brightness = curLevel; saveBrightAt = millis() + 5000; }
    if (curLevel > 0) {
        ui.toastBrightness = curLevel;
        toastUntil = millis() + 1500;
        needRender = true;
    }
}

static void handleBootButton() {
    static bool held = false;
    static unsigned long since = 0;
    bool pressed = !digitalRead(BTN_BOOT);

    if (pressed) {
        if (!held) { held = true; since = millis(); }
        unsigned long d = millis() - since;
        if (d >= 5000) factoryReset();
        if (d >= 800) {
            int8_t left = (int8_t)((5000 - d + 999) / 1000);
            if (left != ui.holdSecondsLeft) { ui.holdSecondsLeft = left; needRender = true; }
        }
    } else if (held) {
        held = false;
        lastInteraction = millis();
        if (ui.holdSecondsLeft >= 0) { ui.holdSecondsLeft = -1; needRender = true; }
        else if (millis() - since >= 30) cycleBrightness();
    }
}

// ── Night dimming ─────────────────────────────────────
static void updateNight() {
    bool n = false;
    if (cfg.nightOn && cfg.tz[0] && timeValid() && cfg.nightFrom != cfg.nightTo) {
        time_t t = time(nullptr);
        struct tm lt; localtime_r(&t, &lt);
        int h = lt.tm_hour;
        n = cfg.nightFrom < cfg.nightTo ? (h >= cfg.nightFrom && h < cfg.nightTo)
                                        : (h >= cfg.nightFrom || h < cfg.nightTo);
    }
    if (n != inNight) {
        inNight  = n;
        curLevel = n ? cfg.nightLevel : cfg.brightness;
        Serial.printf("[UI] night mode %s\n", n ? "on" : "off");
    }
}

// ── Screens shown outside the dashboard ───────────────
void showBoot(int pct, const char* msg, const char* sub) {
    ui.screen  = SCR_BOOT;
    ui.bootPct = pct;
    strlcpy(ui.bootMsg, msg, sizeof(ui.bootMsg));
    strlcpy(ui.bootSub, sub, sizeof(ui.bootSub));
    displayRender();
}

void showMessage(const char* title, const char* detail, const char* hint, bool isError) {
    ui.screen     = SCR_MESSAGE;
    ui.msgIsError = isError;
    strlcpy(ui.msgTitle, title, sizeof(ui.msgTitle));
    strlcpy(ui.msgDetail, detail, sizeof(ui.msgDetail));
    strlcpy(ui.msgHint, hint, sizeof(ui.msgHint));
    displayRender();
}

// Wait while still honouring the BOOT button (so reset always works)
static void idleFor(unsigned long ms) {
    unsigned long t0 = millis();
    while (millis() - t0 < ms) {
        handleBootButton();
        applyBacklight();
        if (needRender) { needRender = false; displayRender(); }
        delay(20);
    }
}

// ── Wi-Fi & time ──────────────────────────────────────
static String savedSsid, savedPass;

static bool connectWiFiBoot() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    for (int tick = 0; WiFi.status() != WL_CONNECTED; tick++) {
        if (tick > WIFI_CONNECT_TIMEOUT_S * 2) return false;
        char sub[48];
        snprintf(sub, sizeof(sub), "%s \xC2\xB7 %ds", savedSsid.c_str(), tick / 2);
        if (tick % 2 == 0) showBoot(40 + tick, "Connecting to Wi-Fi", sub);
        idleFor(500);
    }
    return true;
}

static bool ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.println("[NET] Wi-Fi down, reconnecting");
    WiFi.disconnect();
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        handleBootButton();
        delay(50);
    }
    return WiFi.status() == WL_CONNECTED;
}

static void startTimeSync() {
    configTzTime(cfg.tz[0] ? cfg.tz : "UTC0", "pool.ntp.org", "time.nist.gov");
}

// ── Fetch ─────────────────────────────────────────────
static void doFetch() {
    ensureWiFi();
    fetchUsage(token, usage);
    lastFetchMs = millis();

    if (usage.ok) {
        failStreak     = 0;
        ui.hasData     = true;
        ui.h5          = usage.h5;
        ui.d7          = usage.d7;
        ui.h5Reset     = usage.h5ResetEpoch;
        ui.d7Reset     = usage.d7ResetEpoch;
        ui.limited     = usage.limited;
        ui.net         = NET_OK;
        ui.netDetail[0] = 0;
        ui.lastOkEpoch = timeValid() ? (uint32_t)time(nullptr) : 0;
        nextFetchInMs  = (unsigned long)cfg.pollSec * 1000UL;
    } else {
        failStreak++;
        if (usage.httpCode == 401 || usage.httpCode == 403) ui.net = NET_AUTH;
        else if (usage.httpCode <= 0)                        ui.net = NET_OFFLINE;
        else                                                 ui.net = NET_ERROR;
        strlcpy(ui.netDetail, usage.error, sizeof(ui.netDetail));
        // retry sooner after a hiccup, then fall back to the normal interval
        nextFetchInMs = (failStreak <= 3 ? 30UL : (unsigned long)cfg.pollSec) * 1000UL;
        Serial.printf("[API] failed (%d): %s\n", usage.httpCode, usage.error);
    }

    if (WiFi.status() == WL_CONNECTED) {
        ui.rssi = WiFi.RSSI();
        strlcpy(ui.ip, WiFi.localIP().toString().c_str(), sizeof(ui.ip));
    } else {
        ui.rssi = 0;
    }
    needRender = true;
}

void requestRefresh() { refreshWanted = true; }

// ── Setup ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.printf("\nClaude Monitor v%s\n", FW_VERSION);

    lcd.init();
    lcd.setRotation(SCREEN_ROT);
    pinMode(BTN_BOOT, INPUT_PULLUP);
    ledcAttach(BL_PIN, 5000, 8);
    displayInit();
    touchInit();

    showBoot(10, "Starting up", "");
    setBacklightRaw(DEFAULT_BRIGHTNESS);

    prefs.begin(NVS_NAMESPACE, true);
    bool provisioned = prefs.getBool("provisioned", false);
    prefs.end();

    if (!provisioned) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char apName[24];
        snprintf(apName, sizeof(apName), "ClaudeMonitor-%02X%02X", mac[4], mac[5]);

        static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        uint8_t rnd[8];
        esp_fill_random(rnd, sizeof(rnd));
        char apPass[9];
        for (int i = 0; i < 8; i++) apPass[i] = alphabet[rnd[i] % (sizeof(alphabet) - 1)];
        apPass[8] = '\0';

        runProvisioningPortal(apName, apPass);   // never returns
    }

    loadSettings();
    applyTimezone();
    curLevel = cfg.brightness;
    syncUiFromSettings();
    applyBacklight();

    prefs.begin(NVS_NAMESPACE, true);
    savedSsid = prefs.getString("ssid", "");
    savedPass = prefs.getString("wifipass", "");
    EncryptedBlob blob;
    memset(&blob, 0, sizeof(blob));
    prefs.getBytes("blob", &blob, sizeof(blob));
    prefs.end();

    if (!decryptToken(blob, token, sizeof(token))) {
        showMessage("Token unreadable", "The saved token couldn't be read.", "Hold BOOT 5 s to set up again", true);
        while (true) idleFor(1000);
    }

    showBoot(30, "Connecting to Wi-Fi", savedSsid.c_str());
    while (!connectWiFiBoot()) {
        char detail[96];
        snprintf(detail, sizeof(detail), "Couldn't join %s. Trying again in 30 s.", savedSsid.c_str());
        showMessage("Wi-Fi not found", detail, "Hold BOOT 5 s to change Wi-Fi", true);
        idleFor(30000);
    }
    Serial.printf("[NET] connected, IP %s\n", WiFi.localIP().toString().c_str());

    showBoot(80, "Setting the clock", "");
    startTimeSync();
    for (int i = 0; i < 50 && !timeValid(); i++) idleFor(100);

    webBegin();

    showBoot(95, "Fetching usage", "");
    doFetch();

    ui.screen = SCR_DASH;
    ui.page   = cfg.startPage;
    lastInteraction = millis();
    needRender = true;
}

// ── Loop ──────────────────────────────────────────────
void loop() {
    handleBootButton();
    handleTouch();
    webLoop();
    updateNight();

    // Fetch on schedule, or right away when asked
    if (refreshWanted) {
        refreshWanted = false;
        ui.toastRefresh = true;
        displayRender();
        doFetch();
        ui.toastRefresh = false;
        needRender = true;
    } else if (millis() - lastFetchMs >= nextFetchInMs) {
        doFetch();
    }

    // Redraw when the minute changes (clock, countdowns)
    static long lastMinute = -1;
    long minute = (long)(time(nullptr) / 60);
    if (minute != lastMinute) { lastMinute = minute; needRender = true; }

    // Toasts
    if (toastUntil && millis() > toastUntil) {
        toastUntil = 0;
        ui.toastBrightness = -1;
        needRender = true;
    }

    // Save brightness a few seconds after the last button press
    if (saveBrightAt && millis() > saveBrightAt) {
        saveBrightAt = 0;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putInt("brightness", cfg.brightness);
        prefs.end();
    }

    // Auto-cycle Overview → 5-hour → Weekly when left alone
    if (cfg.rotateSec && ui.screen == SCR_DASH && millis() - lastInteraction > 30000 &&
        millis() - lastRotate > cfg.rotateSec * 1000UL) {
        lastRotate = millis();
        ui.page = (ui.page + 1) % PAGE_DEVICE;
        needRender = true;
    }

    applyBacklight();
    if (needRender) { needRender = false; displayRender(); }
    delay(10);
}
