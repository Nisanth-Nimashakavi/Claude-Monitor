// ═══════════════════════════════════════════════════════
//  display.ino — pushes frames to the panel
//
//  The whole screen is redrawn in 40-row bands: each band is
//  drawn in RAM (25 KB) and then sent to the panel in one go.
//  The panel never shows a half-drawn or blank frame, so
//  there is no flicker.
// ═══════════════════════════════════════════════════════

static const int BAND_ROWS = 40;
static uint16_t* bandBuf  = nullptr;
static int       bandRows = BAND_ROWS;

void displayInit() {
    bandBuf = (uint16_t*)malloc(SCREEN_W * BAND_ROWS * sizeof(uint16_t));
    if (!bandBuf) {                       // very unlikely; fall back to thin bands
        bandRows = 8;
        bandBuf  = (uint16_t*)malloc(SCREEN_W * bandRows * sizeof(uint16_t));
    }
    lcd.setSwapBytes(true);               // buffer is native-endian RGB565
    lcd.fillScreen(TFT_BLACK);
}

void displayRender() {
    if (!bandBuf) return;
    time_t t = time(nullptr);
    ui.now = (t > 1700000000) ? (uint32_t)t : 0;

    static int frames = 0;
    unsigned long t0 = millis();
    Canvas c;
    lcd.startWrite();
    for (int y = 0; y < SCREEN_H; y += bandRows) {
        int h = (SCREEN_H - y < bandRows) ? SCREEN_H - y : bandRows;
        c.begin(bandBuf, 0, y, SCREEN_W, h);
        drawScene(c, ui);
        lcd.pushImage(0, y, SCREEN_W, h, bandBuf);
    }
    lcd.endWrite();
    if (frames++ < 5) Serial.printf("[UI] frame drawn in %lu ms\n", millis() - t0);
}
