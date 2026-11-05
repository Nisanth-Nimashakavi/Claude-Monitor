// ═══════════════════════════════════════════════════════
//  screens.cpp — layouts for every screen and page
//  Screen is 320 x 240. All text is at least 19 px tall.
// ═══════════════════════════════════════════════════════
#include "screens.h"
#include "fonts.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

// ── Palette: warm charcoal, off-white, coral accent ────
namespace pal {
const color_t BG     = RGB(0x121110);
const color_t CARD   = RGB(0x1F1D1B);
const color_t TRACK  = RGB(0x2D2A27);
const color_t LINE   = RGB(0x3A3632);
const color_t TEXT   = RGB(0xF4F0E8);
const color_t DIM    = RGB(0xADA79E);
const color_t FAINT  = RGB(0x78726A);
const color_t ACCENT = RGB(0xE5825F);
const color_t OK     = RGB(0x66C792);
const color_t WARN   = RGB(0xF4B740);
const color_t CRIT   = RGB(0xF4675F);
}  // namespace pal

static const uint32_t WIN_5H = 5UL * 3600UL;
static const uint32_t WIN_7D = 7UL * 86400UL;

static const int MARGIN = 16;
static const int HEADER_BASE = 25;   // header text baseline

// ── Formatting helpers ─────────────────────────────────
static void fmtDuration(uint32_t s, char* out, size_t n) {
    if (s < 60) { snprintf(out, n, "<1m"); return; }
    unsigned d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
    if (d)      snprintf(out, n, "%ud %uh", d, h);
    else if (h) snprintf(out, n, "%uh %02um", h, m);
    else        snprintf(out, n, "%um", m);
}

static void fmtAgo(uint32_t s, char* out, size_t n) {
    if (s < 60)        snprintf(out, n, "just now");
    else if (s < 3600) snprintf(out, n, "%um ago", (unsigned)(s / 60));
    else               snprintf(out, n, "%uh %um ago", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
}

static void localTm(uint32_t t, struct tm& out) {
    time_t tt = (time_t)t;
    localtime_r(&tt, &out);
}

// "4:32" + "PM"   (or "16:32" + "")
static void fmtClock(uint32_t t, bool h24, char* hm, size_t n, char* ap, size_t an) {
    struct tm lt; localTm(t, lt);
    if (h24) { snprintf(hm, n, "%02d:%02d", lt.tm_hour, lt.tm_min); ap[0] = 0; return; }
    int h = lt.tm_hour % 12; if (!h) h = 12;
    snprintf(hm, n, "%d:%02d", h, lt.tm_min);
    snprintf(ap, an, "%s", lt.tm_hour < 12 ? "AM" : "PM");
}

// "4:30 PM", "4 PM", "Thu 9 AM", "Thu 09:00"
static void fmtWhen(uint32_t t, uint32_t now, bool h24, char* out, size_t n) {
    static const char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm lt, ln; localTm(t, lt); localTm(now, ln);
    char tod[12];
    if (h24) snprintf(tod, sizeof(tod), "%02d:%02d", lt.tm_hour, lt.tm_min);
    else {
        int h = lt.tm_hour % 12; if (!h) h = 12;
        const char* ap = lt.tm_hour < 12 ? "AM" : "PM";
        if (lt.tm_min) snprintf(tod, sizeof(tod), "%d:%02d %s", h, lt.tm_min, ap);
        else           snprintf(tod, sizeof(tod), "%d %s", h, ap);
    }
    bool sameDay = lt.tm_yday == ln.tm_yday && lt.tm_year == ln.tm_year;
    if (sameDay) snprintf(out, n, "%s", tod);
    else         snprintf(out, n, "%s %s", DAYS[lt.tm_wday], tod);
}

// ── Pace / projection ──────────────────────────────────
Pace computePace(float used, uint32_t reset, uint32_t now, uint32_t win) {
    Pace p = {};
    if (!now || !reset) return p;
    uint32_t remain = reset > now ? reset - now : 0;
    if (remain > win) remain = win;
    p.timeKnown   = true;
    p.secsToReset = remain;
    uint32_t el   = win - remain;
    p.elapsed     = (float)el / (float)win;
    if (el >= win * 0.15f && used >= 1.0f) {
        p.projValid = true;
        float rate  = used / (float)el;                    // % per second
        p.projected = used + rate * (float)remain;
        if (p.projected >= 100.0f && used < 100.0f)
            p.secsToLimit = (uint32_t)((100.0f - used) / rate);
    }
    return p;
}

color_t levelColor(float used, const Pace& p, bool limited) {
    if (limited || used >= 90.0f) return pal::CRIT;
    if (used >= 75.0f) return pal::WARN;
    if (p.projValid && p.secsToLimit > 0) return pal::WARN;
    return pal::OK;
}

// ── Small building blocks ──────────────────────────────
static void wrapCentered(Canvas& c, const GfxFont& f, int cx, int y, int maxW, int lineH,
                         const char* s, color_t col, int maxLines) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", s);
    char* p = buf;
    for (int line = 0; line < maxLines && *p; line++) {
        // take as many words as fit
        char* best = nullptr;
        char* q = p;
        while (true) {
            char* sp = strchr(q, ' ');
            char save = 0;
            if (sp) { save = *sp; *sp = 0; }
            bool fits = c.textWidth(f, p) <= maxW;
            if (sp) *sp = save;
            if (!fits) break;
            best = sp;          // may be null = whole rest fits
            if (!sp) { best = p + strlen(p); break; }
            q = sp + 1;
        }
        if (!best) best = strchr(p, ' ') ? strchr(p, ' ') : p + strlen(p);   // force one word
        char save = *best; *best = 0;
        c.text(f, cx, y + line * lineH, p, col, ALIGN_CENTER);
        *best = save;
        p = best;
        while (*p == ' ') p++;
    }
}

static void drawHeader(Canvas& c, const Model& m, const char* title) {
    if (!c.rowsVisible(0, 36)) return;

    // Left: status if something is wrong, otherwise the page title
    char left[48];
    color_t dot = pal::ACCENT;
    color_t txt = pal::TEXT;
    if (m.net == NET_OFFLINE || m.net == NET_ERROR || m.net == NET_AUTH) {
        dot = (m.net == NET_AUTH) ? pal::CRIT : pal::WARN;
        txt = dot;
        const char* what = (m.net == NET_OFFLINE) ? "Offline" : (m.net == NET_AUTH) ? "Token problem" : "Update failed";
        if (m.hasData && m.now && m.lastOkEpoch && m.now > m.lastOkEpoch) {
            char ago[20]; fmtAgo(m.now - m.lastOkEpoch, ago, sizeof(ago));
            snprintf(left, sizeof(left), "%s \xC2\xB7 %s", what, ago);
        } else {
            snprintf(left, sizeof(left), "%s", what);
        }
    } else {
        snprintf(left, sizeof(left), "%s", title);
    }
    c.fillCircle(MARGIN + 5, HEADER_BASE - 8, 5, dot);
    c.text(F_BODY, MARGIN + 18, HEADER_BASE, left, txt);

    // Right: clock
    if (m.tzSet && m.now) {
        char hm[8], ap[4];
        fmtClock(m.now, m.clock24, hm, sizeof(hm), ap, sizeof(ap));
        int x = SCREEN_W - MARGIN;
        if (ap[0]) {
            c.text(F_SMALL, x, HEADER_BASE, ap, pal::DIM, ALIGN_RIGHT);
            x -= c.textWidth(F_SMALL, ap) + 5;
        }
        c.text(F_BODY, x, HEADER_BASE, hm, pal::TEXT, ALIGN_RIGHT);
    }
}

static void ringGauge(Canvas& c, const Model& m, int cx, int cy, float used, uint32_t reset,
                      uint32_t win, const char* label) {
    const float R = 66, T = 12;
    if (!c.rowsVisible(cy - R - 10, cy + R + 10)) return;
    Pace p = computePace(used, reset, m.now, win);
    bool limited = m.limited && used >= 99.5f;
    color_t col = levelColor(used, p, limited);

    c.ring(cx, cy, R, T, pal::TRACK);
    float pct = used < 0 ? 0 : (used > 100 ? 100 : used);
    if (pct > 0.2f) c.arc(cx, cy, R, T, 0, pct * 3.6f < 3 ? 3 : pct * 3.6f, col);

    char num[8]; snprintf(num, sizeof(num), "%d", (int)lroundf(pct));
    int wN = c.textWidth(F_HERO, num), wP = c.textWidth(F_LARGE, "%");
    bool showPct = (wN + wP + 1) <= (int)(2 * (R - T / 2) - 10);
    int total = wN + (showPct ? wP + 1 : 0);
    int x0 = cx - total / 2, base = cy + 14;
    c.text(F_HERO, x0, base, num, pal::TEXT);
    if (showPct) c.text(F_LARGE, x0 + wN + 1, base, "%", pal::DIM);
    c.text(F_SMALL, cx, base + 24, label, pal::DIM, ALIGN_CENTER);
}

// ── Pages ──────────────────────────────────────────────
static void pageOverview(Canvas& c, const Model& m) {
    drawHeader(c, m, "Claude usage");
    const int cy = 110;
    ringGauge(c, m, 80, cy, m.h5, m.h5Reset, WIN_5H, "5-hour");
    ringGauge(c, m, 240, cy, m.d7, m.d7Reset, WIN_7D, "Weekly");

    if (!c.rowsVisible(186, 240)) return;
    const uint32_t resets[2] = {m.h5Reset, m.d7Reset};
    const int xs[2] = {80, 240};
    for (int i = 0; i < 2; i++) {
        char v[20] = "--";
        if (m.now && resets[i]) fmtDuration(resets[i] > m.now ? resets[i] - m.now : 0, v, sizeof(v));
        c.text(F_SMALL, xs[i], 205, "Resets in", pal::DIM, ALIGN_CENTER);
        c.text(F_LARGE, xs[i], 235, v, pal::TEXT, ALIGN_CENTER);
    }
}

static void pageFocus(Canvas& c, const Model& m, bool week) {
    float used     = week ? m.d7 : m.h5;
    uint32_t reset = week ? m.d7Reset : m.h5Reset;
    uint32_t win   = week ? WIN_7D : WIN_5H;
    drawHeader(c, m, week ? "Weekly limit" : "5-hour limit");

    Pace p = computePace(used, reset, m.now, win);
    bool limited = m.limited && used >= 99.5f;
    color_t col = levelColor(used, p, limited);
    float pct = used < 0 ? 0 : (used > 100 ? 100 : used);

    // Giant number
    char num[8]; snprintf(num, sizeof(num), "%d", (int)lroundf(pct));
    const int base = 134;
    int x = c.text(F_GIANT, MARGIN - 4, base, num, pal::TEXT);
    c.text(F_HERO, x + 2, base, "%", pal::DIM);

    // Bar with time-elapsed marker
    const int bx = MARGIN, by = 150, bw = SCREEN_W - 2 * MARGIN, bh = 16;
    if (c.rowsVisible(by - 6, by + bh + 6)) {
        c.fillRoundRect(bx, by, bw, bh, bh / 2, pal::TRACK);
        if (pct > 0.2f) {
            float fw = bw * pct / 100.0f;
            if (fw < bh) fw = bh;
            c.fillRoundRect(bx, by, fw, bh, bh / 2, col);
        }
        if (p.timeKnown) {
            float tx = bx + bw * p.elapsed;
            if (tx < bx + 3) tx = bx + 3;
            if (tx > bx + bw - 3) tx = bx + bw - 3;
            c.line(tx, by - 4, tx, by + bh + 4, 5.0f, pal::BG);
            c.line(tx, by - 4, tx, by + bh + 4, 2.2f, pal::TEXT);
        }
    }

    // Reset row
    char dur[20] = "--", when[24] = "";
    if (p.timeKnown) fmtDuration(p.secsToReset, dur, sizeof(dur));
    if (m.tzSet && m.now && reset) fmtWhen(reset, m.now, m.clock24, when, sizeof(when));
    // "Resets in 2h 14m      6:46 PM"  or, when that's too wide,
    // "Resets Fri 9 AM          2d 5h"
    char left[48], right[24];
    snprintf(left, sizeof(left), "Resets in %s", dur);
    snprintf(right, sizeof(right), "%s", when);
    const int avail = SCREEN_W - 2 * MARGIN - 14;
    if (when[0] && c.textWidth(F_BODY, left) + c.textWidth(F_BODY, right) > avail) {
        snprintf(left, sizeof(left), "Resets %s", when);
        snprintf(right, sizeof(right), "%s", dur);
        if (c.textWidth(F_BODY, left) + c.textWidth(F_BODY, right) > avail) right[0] = 0;
    }
    c.text(F_BODY, MARGIN, 196, left, pal::TEXT);
    if (right[0]) c.text(F_BODY, SCREEN_W - MARGIN, 196, right, pal::DIM, ALIGN_RIGHT);

    // Pace row
    char pace[48]; color_t pc = pal::DIM;
    if (limited || pct >= 99.5f) {
        snprintf(pace, sizeof(pace), "Limit reached"); pc = pal::CRIT;
    } else if (!m.now) {
        snprintf(pace, sizeof(pace), "Waiting for clock");
    } else if (!p.timeKnown) {
        snprintf(pace, sizeof(pace), "No reset time yet");
    } else if (!p.projValid) {
        snprintf(pace, sizeof(pace), "Too early to project");
    } else if (p.secsToLimit) {
        char d[20]; fmtDuration(p.secsToLimit, d, sizeof(d));
        snprintf(pace, sizeof(pace), "Runs out in ~%s", d); pc = col == pal::OK ? pal::WARN : col;
    } else {
        snprintf(pace, sizeof(pace), "On pace for ~%d%%", (int)lroundf(p.projected)); pc = pal::OK;
    }
    if (c.rowsVisible(206, 240)) {
        c.fillCircle(MARGIN + 5, 220, 5, pc);
        c.text(F_BODY, MARGIN + 18, 228, pace, pc == pal::DIM ? pal::DIM : pal::TEXT);
    }
}

static void row(Canvas& c, int y, const char* label, const char* value, color_t vc = pal::TEXT) {
    c.text(F_SMALL, MARGIN, y, label, pal::DIM);
    c.text(F_BODY, SCREEN_W - MARGIN, y, value, vc, ALIGN_RIGHT);
}

static void pageDevice(Canvas& c, const Model& m) {
    drawHeader(c, m, "Device");

    char host[48]; snprintf(host, sizeof(host), "%s.local", m.host);
    c.text(F_SMALL, MARGIN, 64, m.tzSet ? "Settings page" : "Set your time zone at", m.tzSet ? pal::DIM : pal::WARN);
    c.text(F_BODY, MARGIN, 92, host, pal::ACCENT);
    if (m.ip[0]) {
        char alt[40]; snprintf(alt, sizeof(alt), "or http://%s", m.ip);
        c.text(F_SMALL, MARGIN, 118, alt, pal::DIM);
    }
    c.fillRect(MARGIN, 132, SCREEN_W - 2 * MARGIN, 1, pal::LINE);

    char v[40];
    const char* q = m.rssi == 0 ? "--" : m.rssi > -60 ? "Strong" : m.rssi > -70 ? "Good" : m.rssi > -80 ? "Fair" : "Weak";
    if (m.rssi) snprintf(v, sizeof(v), "%s \xC2\xB7 %d dBm", q, m.rssi); else snprintf(v, sizeof(v), "Not connected");
    row(c, 162, "Wi-Fi", v, m.rssi && m.rssi <= -80 ? pal::WARN : pal::TEXT);

    if (m.lastOkEpoch && m.now >= m.lastOkEpoch) fmtAgo(m.now - m.lastOkEpoch, v, sizeof(v));
    else snprintf(v, sizeof(v), "Not yet");
    row(c, 194, "Last update", v);

    if (m.pollSec % 60 == 0) snprintf(v, sizeof(v), "Every %d min \xC2\xB7 v%s", m.pollSec / 60, m.version);
    else snprintf(v, sizeof(v), "Every %ds \xC2\xB7 v%s", m.pollSec, m.version);
    row(c, 226, "Refresh", v);
}

static void pageNoData(Canvas& c, const Model& m) {
    drawHeader(c, m, "Claude usage");
    const char* title = m.net == NET_LOADING ? "Fetching usage" : m.net == NET_AUTH ? "Token rejected" : "Can't reach Claude";
    color_t ic = m.net == NET_LOADING ? pal::ACCENT : m.net == NET_AUTH ? pal::CRIT : pal::WARN;
    if (c.rowsVisible(50, 110)) {
        c.ring(160, 80, 22, 5, ic);
        if (m.net != NET_LOADING) {
            c.line(160, 69, 160, 83, 5, ic);
            c.fillCircle(160, 91.5f, 3, ic);
        }
    }
    c.text(F_LARGE, 160, 142, title, pal::TEXT, ALIGN_CENTER);
    const char* d = m.net == NET_AUTH ? "Hold the BOOT button for 5 seconds to set up again."
                                      : (m.netDetail[0] ? m.netDetail : "Trying again shortly.");
    wrapCentered(c, F_BODY, 160, 176, 288, 28, d, pal::DIM, 2);
}

// ── Non-dashboard screens ──────────────────────────────
static void screenBoot(Canvas& c, const Model& m) {
    if (c.rowsVisible(40, 110)) {
        c.ring(160, 70, 24, 7, pal::TRACK);
        c.arc(160, 70, 24, 7, 0, 3.6f * (m.bootPct < 4 ? 4 : m.bootPct), pal::ACCENT);
    }
    c.text(F_LARGE, 160, 136, "Claude Monitor", pal::TEXT, ALIGN_CENTER);
    c.text(F_BODY, 160, 176, m.bootMsg, pal::DIM, ALIGN_CENTER);
    if (m.bootSub[0]) c.text(F_SMALL, 160, 206, m.bootSub, pal::FAINT, ALIGN_CENTER);
}

static void screenSetup(Canvas& c, const Model& m) {
    c.fillCircle(MARGIN + 5, HEADER_BASE - 8, 5, pal::ACCENT);
    c.text(F_BODY, MARGIN + 18, HEADER_BASE, "Let's get set up", pal::TEXT);

    c.fillRoundRect(MARGIN - 4, 38, SCREEN_W - 2 * MARGIN + 8, 124, 12, pal::CARD);
    c.text(F_SMALL, MARGIN + 8, 62, "1  On your phone, join Wi-Fi", pal::DIM);
    c.text(F_BODY, MARGIN + 8, 92, m.apName, pal::TEXT);
    c.text(F_SMALL, MARGIN + 8, 122, "Password", pal::DIM);
    c.text(F_LARGE, MARGIN + 8, 152, m.apPass, pal::ACCENT);

    c.text(F_SMALL, MARGIN + 8, 190, "2  Then open in a browser", pal::DIM);
    c.text(F_LARGE, MARGIN + 8, 225, "192.168.4.1", pal::TEXT);
}

static void screenMessage(Canvas& c, const Model& m) {
    color_t ic = m.msgIsError ? pal::CRIT : pal::ACCENT;
    if (c.rowsVisible(20, 90)) {
        c.ring(160, 50, 22, 5, ic);
        c.line(160, 39, 160, 53, 5, ic);
        c.fillCircle(160, 61.5f, 3, ic);
    }
    c.text(F_LARGE, 160, 114, m.msgTitle, pal::TEXT, ALIGN_CENTER);
    wrapCentered(c, F_BODY, 160, 148, 290, 27, m.msgDetail, pal::DIM, 3);
    if (m.msgHint[0]) wrapCentered(c, F_SMALL, 160, 230, 300, 22, m.msgHint, pal::FAINT, 1);
}

// ── Overlays ───────────────────────────────────────────
static void overlayHold(Canvas& c, const Model& m) {
    c.fillRect(0, 0, SCREEN_W, SCREEN_H, pal::BG);
    c.fillRoundRect(28, 44, SCREEN_W - 56, 152, 16, pal::CARD);
    c.text(F_LARGE, 160, 90, "Factory reset", pal::CRIT, ALIGN_CENTER);
    char s[32]; snprintf(s, sizeof(s), "Keep holding %ds", m.holdSecondsLeft);
    c.text(F_BODY, 160, 130, s, pal::TEXT, ALIGN_CENTER);
    c.text(F_SMALL, 160, 170, "Let go to cancel", pal::DIM, ALIGN_CENTER);
}

static void toast(Canvas& c, int w, void (*content)(Canvas&, const Model&, int, int), const Model& m) {
    const int h = 40, y = SCREEN_H - h - 10, x = (SCREEN_W - w) / 2;
    if (!c.rowsVisible(y, y + h)) return;
    c.fillRoundRect(x, y, w, h, h / 2, pal::LINE);
    content(c, m, x, y);
}

static void toastBrightContent(Canvas& c, const Model& m, int x, int y) {
    c.text(F_SMALL, x + 20, y + 27, "Brightness", pal::TEXT);
    for (int i = 0; i < 3; i++) {
        float cx = x + 132 + i * 18, cy = y + 20;
        if (i < m.toastBrightness) c.fillCircle(cx, cy, 6, pal::ACCENT);
        else c.ring(cx, cy, 5, 2, pal::DIM);
    }
}

static void toastRefreshContent(Canvas& c, const Model&, int x, int y) {
    c.text(F_SMALL, x + 80, y + 27, "Refreshing\xE2\x80\xA6", pal::TEXT, ALIGN_CENTER);
}

// ── Entry point ────────────────────────────────────────
void drawScene(Canvas& c, const Model& m) {
    c.clear(pal::BG);
    switch (m.screen) {
        case SCR_BOOT:    screenBoot(c, m); break;
        case SCR_SETUP:   screenSetup(c, m); break;
        case SCR_MESSAGE: screenMessage(c, m); break;
        case SCR_DASH:
            if (m.page == PAGE_DEVICE)       pageDevice(c, m);
            else if (!m.hasData)             pageNoData(c, m);
            else if (m.page == PAGE_SESSION) pageFocus(c, m, false);
            else if (m.page == PAGE_WEEK)    pageFocus(c, m, true);
            else                             pageOverview(c, m);
            break;
    }
    if (m.holdSecondsLeft >= 0) overlayHold(c, m);
    else if (m.toastBrightness > 0) toast(c, 200, toastBrightContent, m);
    else if (m.toastRefresh) toast(c, 160, toastRefreshContent, m);
}
