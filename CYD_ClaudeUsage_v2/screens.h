// ═══════════════════════════════════════════════════════
//  screens.h — everything the display shows, described as
//  one plain struct (Model). drawScene() turns a Model into
//  pixels. Portable: also used by the Mac preview simulator.
// ═══════════════════════════════════════════════════════
#pragma once
#include <stdint.h>
#include "gfx.h"

#define SCREEN_W 320
#define SCREEN_H 240

enum Screen : uint8_t { SCR_BOOT, SCR_SETUP, SCR_MESSAGE, SCR_DASH };

enum Page : uint8_t { PAGE_OVERVIEW = 0, PAGE_SESSION, PAGE_WEEK, PAGE_DEVICE, PAGE_COUNT };

enum NetStatus : uint8_t {
    NET_OK = 0,      // last fetch succeeded
    NET_LOADING,     // no fetch finished yet
    NET_OFFLINE,     // Wi-Fi down or API unreachable
    NET_AUTH,        // token rejected (401/403)
    NET_ERROR,       // anything else
};

struct Model {
    Screen  screen = SCR_BOOT;
    uint8_t page   = PAGE_OVERVIEW;

    // Boot / progress screen
    int  bootPct = 0;
    char bootMsg[48] = "";
    char bootSub[48] = "";

    // Setup (access point) screen
    char apName[24] = "";
    char apPass[12] = "";

    // Full-screen message
    bool msgIsError = true;
    char msgTitle[40]  = "";
    char msgDetail[96] = "";
    char msgHint[64]   = "";

    // Usage data (percent 0-100, reset times as unix epoch)
    bool     hasData = false;
    float    h5 = 0, d7 = 0;
    uint32_t h5Reset = 0, d7Reset = 0;
    bool     limited = false;          // API says requests are being rejected

    // Clock
    uint32_t now = 0;                  // unix epoch, 0 = not synced yet
    bool     tzSet = false;            // a time zone has been chosen
    bool     clock24 = false;

    // Connection / fetch status
    NetStatus net = NET_LOADING;
    char      netDetail[48] = "";
    uint32_t  lastOkEpoch = 0;         // when the shown data was fetched
    int       pollSec = 120;

    // Device info
    int  rssi = 0;
    char ip[16] = "";
    char host[32] = "claude-monitor";
    const char* version = "";

    // Overlays
    int8_t holdSecondsLeft = -1;       // factory-reset hold countdown, -1 = hidden
    int8_t toastBrightness = -1;       // brightness level just chosen, -1 = hidden
    bool   toastRefresh    = false;    // "Refreshing…" pill
};

// Pace: how usage compares with time elapsed in the window.
struct Pace {
    bool     timeKnown;     // reset time & clock are known
    float    elapsed;       // 0-1 fraction of window gone
    bool     projValid;     // enough of the window gone to extrapolate
    float    projected;     // % expected at reset at the current average rate
    uint32_t secsToLimit;   // >0 if projected to hit 100% before reset
    uint32_t secsToReset;
};

Pace     computePace(float used, uint32_t resetEpoch, uint32_t now, uint32_t windowSec);
color_t  levelColor(float used, const Pace& p, bool limited);

void drawScene(Canvas& c, const Model& m);
