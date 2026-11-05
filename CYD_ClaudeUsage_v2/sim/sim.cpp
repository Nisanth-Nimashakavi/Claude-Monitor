// ═══════════════════════════════════════════════════════
//  sim.cpp — renders every screen to BMP files on a Mac,
//  using exactly the same drawing code as the firmware.
//  Build & run:  sh sim/build.sh
// ═══════════════════════════════════════════════════════
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <string>
#include "../screens.h"

static const int BAND_H = 40;   // same band height as the device
static const int SCALE  = 2;    // preview zoom

static void render(const Model& m, std::vector<uint16_t>& fb) {
    fb.assign(SCREEN_W * SCREEN_H, 0);
    static uint16_t band[SCREEN_W * BAND_H];
    Canvas c;
    for (int y = 0; y < SCREEN_H; y += BAND_H) {
        int h = (SCREEN_H - y < BAND_H) ? SCREEN_H - y : BAND_H;
        c.begin(band, 0, y, SCREEN_W, h);
        drawScene(c, m);
        memcpy(&fb[y * SCREEN_W], band, sizeof(uint16_t) * SCREEN_W * h);
    }
}

static void put32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }

static void writeBMP(const char* path, const std::vector<uint32_t>& rgb, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    int rowBytes = (w * 3 + 3) & ~3;
    fputc('B', f); fputc('M', f);
    put32(f, 54 + rowBytes * h); put32(f, 0); put32(f, 54);
    put32(f, 40); put32(f, w); put32(f, h); put16(f, 1); put16(f, 24);
    put32(f, 0); put32(f, rowBytes * h); put32(f, 2835); put32(f, 2835); put32(f, 0); put32(f, 0);
    std::vector<uint8_t> row(rowBytes, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint32_t p = rgb[y * w + x];
            row[x * 3 + 0] = p & 0xFF; row[x * 3 + 1] = (p >> 8) & 0xFF; row[x * 3 + 2] = (p >> 16) & 0xFF;
        }
        fwrite(row.data(), 1, rowBytes, f);
    }
    fclose(f);
}

static uint32_t to888(uint16_t c) {
    uint32_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}

struct Shot { std::string name; std::vector<uint16_t> fb; };

int main(int argc, char** argv) {
    const char* outDir = argc > 1 ? argv[1] : "sim/out";
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();

    struct tm t = {};
    t.tm_year = 2026 - 1900; t.tm_mon = 10; t.tm_mday = 4; t.tm_hour = 16; t.tm_min = 32; t.tm_isdst = -1;
    uint32_t now = (uint32_t)mktime(&t);

    Model base;
    base.screen = SCR_DASH;
    base.now = now; base.tzSet = true; base.hasData = true; base.net = NET_OK;
    base.lastOkEpoch = now - 40; base.rssi = -58; base.pollSec = 120; base.version = "2.0";
    snprintf(base.ip, sizeof(base.ip), "192.168.1.42");
    base.h5 = 42; base.h5Reset = now + 2 * 3600 + 14 * 60;
    base.d7 = 63; base.d7Reset = now + 2 * 86400 + 5 * 3600;

    std::vector<Shot> shots;
    auto add = [&](const char* name, const Model& m) { Shot s; s.name = name; render(m, s.fb); shots.push_back(s); };

    { Model m = base; add("01_overview", m); }
    { Model m = base; m.h5 = 78; m.d7 = 91; m.h5Reset = now + 3 * 3600 + 30 * 60; add("02_overview_high", m); }
    { Model m = base; m.page = PAGE_SESSION; add("03_session", m); }
    { Model m = base; m.page = PAGE_SESSION; m.h5 = 64; m.h5Reset = now + 3 * 3600 + 30 * 60; add("04_session_fast", m); }
    { Model m = base; m.page = PAGE_WEEK; add("05_week", m); }
    { Model m = base; m.page = PAGE_DEVICE; add("06_device", m); }
    { Model m = base; m.h5 = 100; m.limited = true; m.page = PAGE_SESSION; add("07_session_limit", m); }
    { Model m = base; m.net = NET_OFFLINE; m.lastOkEpoch = now - 12 * 60; add("08_overview_offline", m); }
    { Model m = base; m.hasData = false; m.net = NET_AUTH; add("09_token_rejected", m); }
    { Model m = base; m.holdSecondsLeft = 3; add("10_hold_reset", m); }
    { Model m = base; m.toastBrightness = 2; add("11_brightness", m); }
    { Model m = base; m.h5 = 3; m.d7 = 100; m.limited = true; m.h5Reset = now + 4 * 3600 + 50 * 60; add("12_overview_extremes", m); }
    { Model m = base; m.tzSet = false; m.page = PAGE_DEVICE; add("13_device_no_tz", m); }
    { Model m; m.screen = SCR_BOOT; m.bootPct = 70; snprintf(m.bootMsg, sizeof(m.bootMsg), "Connecting to Wi-Fi");
      snprintf(m.bootSub, sizeof(m.bootSub), "HomeNetwork-2G \xC2\xB7 attempt 3"); add("14_boot", m); }
    { Model m; m.screen = SCR_SETUP; snprintf(m.apName, sizeof(m.apName), "ClaudeMonitor-A1B2");
      snprintf(m.apPass, sizeof(m.apPass), "K7MPXQ4R"); add("15_setup", m); }
    { Model m; m.screen = SCR_MESSAGE; snprintf(m.msgTitle, sizeof(m.msgTitle), "Wi-Fi not found");
      snprintf(m.msgDetail, sizeof(m.msgDetail), "Couldn't join HomeNetwork-2G. Trying again in 30 s.");
      snprintf(m.msgHint, sizeof(m.msgHint), "Hold BOOT 5 s to change Wi-Fi"); add("16_message", m); }
    // Edge cases: widest reset text, 24-hour clock, no clock yet, no reset time
    { Model m = base; m.page = PAGE_WEEK; m.d7 = 18; m.d7Reset = now + 6 * 86400 + 23 * 3600 + 58 * 60; add("17_week_wide", m); }
    { Model m = base; m.page = PAGE_WEEK; m.clock24 = true; add("18_week_24h", m); }
    { Model m = base; m.page = PAGE_SESSION; m.now = 0; add("19_session_noclock", m); }
    { Model m = base; m.page = PAGE_SESSION; m.h5 = 0; m.h5Reset = 0; add("20_session_idle", m); }

    // individual 2x images + a contact sheet (2 columns)
    const int W = SCREEN_W * SCALE, H = SCREEN_H * SCALE, GAP = 24;
    const int cols = 2, rows = (int)(shots.size() + cols - 1) / cols;
    const int SW = cols * W + (cols + 1) * GAP, SH = rows * H + (rows + 1) * GAP;
    std::vector<uint32_t> sheet(SW * SH, 0x3A3A3C);
    for (size_t i = 0; i < shots.size(); i++) {
        std::vector<uint32_t> img(W * H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) img[y * W + x] = to888(shots[i].fb[(y / SCALE) * SCREEN_W + x / SCALE]);
        char path[512]; snprintf(path, sizeof(path), "%s/%s.bmp", outDir, shots[i].name.c_str());
        writeBMP(path, img, W, H);
        int ox = GAP + (int)(i % cols) * (W + GAP), oy = GAP + (int)(i / cols) * (H + GAP);
        for (int y = 0; y < H; y++) memcpy(&sheet[(oy + y) * SW + ox], &img[y * W], W * 4);
    }
    char path[512]; snprintf(path, sizeof(path), "%s/_sheet.bmp", outDir);
    writeBMP(path, sheet, SW, SH);
    printf("rendered %zu screens to %s\n", shots.size(), outDir);
    return 0;
}
