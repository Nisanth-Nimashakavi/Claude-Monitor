// ═══════════════════════════════════════════════════════
//  gfx.h — tiny anti-aliased renderer (portable C++)
//
//  Draws into a horizontal "band" of the screen held in RAM.
//  The scene is drawn once per band, then the band is pushed
//  to the panel. Nothing is ever cleared on the panel itself,
//  so updates are flicker-free.
//
//  No Arduino dependencies: the same code builds on a Mac for
//  the preview simulator in sim/.
// ═══════════════════════════════════════════════════════
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint16_t color_t;   // RGB565

// 0xRRGGBB → RGB565 at compile time
#define RGB(h) ((color_t)(((((h) >> 16) & 0xF8) << 8) | ((((h) >> 8) & 0xFC) << 3) | (((h) & 0xFF) >> 3)))

struct GfxGlyph {
    uint16_t code;
    uint8_t  w, h;
    int8_t   xoff, yoff;     // bitmap top-left relative to pen / baseline
    uint16_t adv4;           // advance in 1/4 pixels
    uint32_t off;            // offset into bitmap data
};

struct GfxFont {
    const uint8_t*  bits;
    const GfxGlyph* glyphs;
    uint16_t        count;
    uint8_t         ascent, descent;
    uint8_t         capHeight, digitHeight;
};

enum Align { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

color_t blend565(color_t bg, color_t fg, uint8_t a);
color_t mix565(color_t a, color_t b, float t);   // t=0 → a, t=1 → b

class Canvas {
public:
    // Attach to a band buffer covering screen rows [y, y+h)
    void begin(uint16_t* buf, int x, int y, int w, int h);

    void clear(color_t c);
    void fillRect(int x, int y, int w, int h, color_t c);
    void fillRoundRect(float x, float y, float w, float h, float r, color_t c, uint8_t alpha = 255);
    void strokeRoundRect(float x, float y, float w, float h, float r, float thick, color_t c);
    void fillCircle(float cx, float cy, float r, color_t c, uint8_t alpha = 255);
    void ring(float cx, float cy, float r, float thick, color_t c);
    // Arc centred on radius r; angles in degrees, 0 = 12 o'clock, clockwise.
    void arc(float cx, float cy, float r, float thick, float startDeg, float sweepDeg, color_t c,
             bool roundCaps = true);
    void line(float x0, float y0, float x1, float y1, float thick, color_t c);

    int  textWidth(const GfxFont& f, const char* s) const;
    // y is the BASELINE. Returns the x just past the last glyph.
    int  text(const GfxFont& f, int x, int y, const char* s, color_t c, Align a = ALIGN_LEFT);

    // Band bounds (screen coordinates)
    int top() const    { return by; }
    int bottom() const { return by + bh; }
    bool rowsVisible(int y0, int y1) const { return y1 > by && y0 < by + bh; }

private:
    uint16_t* buf = nullptr;
    int bx = 0, by = 0, bw = 0, bh = 0;

    inline void plot(int x, int y, color_t c, uint8_t a) {
        if (a == 0) return;
        uint16_t* p = buf + (y - by) * bw + (x - bx);
        *p = (a == 255) ? c : blend565(*p, c, a);
    }
    bool clipBox(float x0, float y0, float x1, float y1, int& ix0, int& iy0, int& ix1, int& iy1) const;
    const GfxGlyph* findGlyph(const GfxFont& f, uint16_t code) const;
};

// Decode one UTF-8 code point and advance the pointer.
uint16_t utf8Next(const char*& s);
