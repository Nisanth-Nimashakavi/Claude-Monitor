// ═══════════════════════════════════════════════════════
//  gfx.cpp — anti-aliased primitives using signed distance
//  fields: every pixel's coverage is computed from its
//  distance to the shape edge, which gives smooth edges on
//  rings, rounded bars and text.
// ═══════════════════════════════════════════════════════
#include "gfx.h"
#include <math.h>
#include <string.h>

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline uint8_t cov(float sd) {               // signed distance → alpha
    float c = 0.5f - sd;
    if (c <= 0.0f) return 0;
    if (c >= 1.0f) return 255;
    return (uint8_t)(c * 255.0f + 0.5f);
}
static inline uint8_t mulA(uint8_t a, uint8_t b) { return (uint8_t)((a * b + 127) / 255); }

color_t blend565(color_t bg, color_t fg, uint8_t a) {
    if (a == 255) return fg;
    if (a == 0) return bg;
    uint32_t ia = 255 - a;
    uint32_t r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * ia + 127) / 255;
    uint32_t g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * ia + 127) / 255;
    uint32_t b = ((fg & 0x1F) * a + (bg & 0x1F) * ia + 127) / 255;
    return (color_t)((r << 11) | (g << 5) | b);
}

color_t mix565(color_t a, color_t b, float t) {
    return blend565(a, b, (uint8_t)(clampf(t, 0, 1) * 255.0f + 0.5f));
}

uint16_t utf8Next(const char*& s) {
    uint8_t c = (uint8_t)*s++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *s) { uint16_t r = ((c & 0x1F) << 6) | (*s++ & 0x3F); return r; }
    if ((c & 0xF0) == 0xE0 && s[0] && s[1]) {
        uint16_t r = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
        s += 2;
        return r;
    }
    return '?';
}

void Canvas::begin(uint16_t* b, int x, int y, int w, int h) { buf = b; bx = x; by = y; bw = w; bh = h; }

void Canvas::clear(color_t c) {
    size_t n = (size_t)bw * bh;
    for (size_t i = 0; i < n; i++) buf[i] = c;
}

bool Canvas::clipBox(float x0, float y0, float x1, float y1, int& ix0, int& iy0, int& ix1, int& iy1) const {
    ix0 = (int)floorf(x0); iy0 = (int)floorf(y0);
    ix1 = (int)ceilf(x1);  iy1 = (int)ceilf(y1);
    if (ix0 < bx) ix0 = bx;
    if (iy0 < by) iy0 = by;
    if (ix1 > bx + bw) ix1 = bx + bw;
    if (iy1 > by + bh) iy1 = by + bh;
    return ix0 < ix1 && iy0 < iy1;
}

void Canvas::fillRect(int x, int y, int w, int h, color_t c) {
    int x0, y0, x1, y1;
    if (!clipBox(x, y, x + w, y + h, x0, y0, x1, y1)) return;
    for (int yy = y0; yy < y1; yy++) {
        uint16_t* p = buf + (yy - by) * bw + (x0 - bx);
        for (int xx = x0; xx < x1; xx++) *p++ = c;
    }
}

static inline float sdRoundRect(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float qx = fabsf(px - cx) - (hw - r);
    float qy = fabsf(py - cy) - (hh - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float outside = sqrtf(ox * ox + oy * oy);
    float inside = qx > qy ? qx : qy;
    if (inside > 0) inside = 0;
    return outside + inside - r;
}

void Canvas::fillRoundRect(float x, float y, float w, float h, float r, color_t c, uint8_t alpha) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    int x0, y0, x1, y1;
    if (!clipBox(x - 1, y - 1, x + w + 1, y + h + 1, x0, y0, x1, y1)) return;
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2;
    for (int yy = y0; yy < y1; yy++) {
        float py = yy + 0.5f;
        for (int xx = x0; xx < x1; xx++) {
            uint8_t a = cov(sdRoundRect(xx + 0.5f, py, cx, cy, hw, hh, r));
            if (a) plot(xx, yy, c, alpha == 255 ? a : mulA(a, alpha));
        }
    }
}

void Canvas::strokeRoundRect(float x, float y, float w, float h, float r, float t, color_t c) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    int x0, y0, x1, y1;
    if (!clipBox(x - 1, y - 1, x + w + 1, y + h + 1, x0, y0, x1, y1)) return;
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2;
    for (int yy = y0; yy < y1; yy++) {
        float py = yy + 0.5f;
        for (int xx = x0; xx < x1; xx++) {
            float sd = sdRoundRect(xx + 0.5f, py, cx, cy, hw, hh, r);
            float s2 = -sd - t;
            plot(xx, yy, c, cov(sd > s2 ? sd : s2));
        }
    }
}

void Canvas::fillCircle(float cx, float cy, float r, color_t c, uint8_t alpha) {
    int x0, y0, x1, y1;
    if (!clipBox(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, x0, y0, x1, y1)) return;
    for (int yy = y0; yy < y1; yy++) {
        float dy = yy + 0.5f - cy;
        for (int xx = x0; xx < x1; xx++) {
            float dx = xx + 0.5f - cx;
            uint8_t a = cov(sqrtf(dx * dx + dy * dy) - r);
            if (a) plot(xx, yy, c, alpha == 255 ? a : mulA(a, alpha));
        }
    }
}

void Canvas::ring(float cx, float cy, float r, float t, color_t c) {
    float ro = r + t / 2 + 1;
    int x0, y0, x1, y1;
    if (!clipBox(cx - ro, cy - ro, cx + ro, cy + ro, x0, y0, x1, y1)) return;
    for (int yy = y0; yy < y1; yy++) {
        float dy = yy + 0.5f - cy;
        for (int xx = x0; xx < x1; xx++) {
            float dx = xx + 0.5f - cx;
            plot(xx, yy, c, cov(fabsf(sqrtf(dx * dx + dy * dy) - r) - t / 2));
        }
    }
}

void Canvas::arc(float cx, float cy, float r, float t, float start, float sweep, color_t c, bool caps) {
    if (sweep <= 0.0f) return;
    if (sweep >= 360.0f) { ring(cx, cy, r, t, c); return; }
    const float D2R = 0.01745329252f;
    float ht = t / 2;
    float s0 = start * D2R, s1 = (start + sweep) * D2R;
    float c0x = cx + r * sinf(s0), c0y = cy - r * cosf(s0);
    float c1x = cx + r * sinf(s1), c1y = cy - r * cosf(s1);
    float ro = r + ht + 1;
    int x0, y0, x1, y1;
    if (!clipBox(cx - ro, cy - ro, cx + ro, cy + ro, x0, y0, x1, y1)) return;
    for (int yy = y0; yy < y1; yy++) {
        float py = yy + 0.5f, dy = py - cy;
        for (int xx = x0; xx < x1; xx++) {
            float px = xx + 0.5f, dx = px - cx;
            float dist = sqrtf(dx * dx + dy * dy);
            float band = fabsf(dist - r) - ht;
            if (band > 1.0f) continue;
            float ang = atan2f(dx, -dy) / D2R;           // 0 at top, clockwise
            float rel = fmodf(ang - start + 720.0f, 360.0f);
            float sd;
            if (rel <= sweep) {
                sd = band;
            } else if (caps) {
                float ax = px - c0x, ay = py - c0y, bx2 = px - c1x, by2 = py - c1y;
                float d0 = sqrtf(ax * ax + ay * ay), d1 = sqrtf(bx2 * bx2 + by2 * by2);
                sd = (d0 < d1 ? d0 : d1) - ht;
            } else {
                // flat ends: distance along the tangent to the nearer end
                float e = (rel - sweep < 360.0f - rel) ? (rel - sweep) : (360.0f - rel);
                float along = e * D2R * dist;
                sd = along > band ? along : band;
            }
            if (caps && rel <= sweep) {
                float ax = px - c0x, ay = py - c0y, bx2 = px - c1x, by2 = py - c1y;
                float d0 = sqrtf(ax * ax + ay * ay) - ht, d1 = sqrtf(bx2 * bx2 + by2 * by2) - ht;
                if (d0 < sd) sd = d0;
                if (d1 < sd) sd = d1;
            }
            plot(xx, yy, c, cov(sd));
        }
    }
}

void Canvas::line(float ax, float ay, float bx2, float by2, float t, color_t c) {
    float ht = t / 2;
    float minx = (ax < bx2 ? ax : bx2) - ht - 1, maxx = (ax > bx2 ? ax : bx2) + ht + 1;
    float miny = (ay < by2 ? ay : by2) - ht - 1, maxy = (ay > by2 ? ay : by2) + ht + 1;
    int x0, y0, x1, y1;
    if (!clipBox(minx, miny, maxx, maxy, x0, y0, x1, y1)) return;
    float bax = bx2 - ax, bay = by2 - ay;
    float bb = bax * bax + bay * bay;
    if (bb < 1e-6f) bb = 1e-6f;
    for (int yy = y0; yy < y1; yy++) {
        float pay = yy + 0.5f - ay;
        for (int xx = x0; xx < x1; xx++) {
            float pax = xx + 0.5f - ax;
            float h = clampf((pax * bax + pay * bay) / bb, 0, 1);
            float dx = pax - bax * h, dy = pay - bay * h;
            plot(xx, yy, c, cov(sqrtf(dx * dx + dy * dy) - ht));
        }
    }
}

const GfxGlyph* Canvas::findGlyph(const GfxFont& f, uint16_t code) const {
    for (uint16_t i = 0; i < f.count; i++)
        if (f.glyphs[i].code == code) return &f.glyphs[i];
    return nullptr;
}

int Canvas::textWidth(const GfxFont& f, const char* s) const {
    int w4 = 0;
    while (*s) {
        const GfxGlyph* g = findGlyph(f, utf8Next(s));
        if (g) w4 += g->adv4;
    }
    return (w4 + 2) / 4;
}

int Canvas::text(const GfxFont& f, int x, int y, const char* s, color_t c, Align al) {
    if (al != ALIGN_LEFT) {
        int w = textWidth(f, s);
        x -= (al == ALIGN_CENTER) ? w / 2 : w;
    }
    int pen4 = x * 4;
    // quick reject when the whole line is outside this band
    if (y - f.ascent > by + bh || y + f.descent < by) {
        while (*s) { const GfxGlyph* g = findGlyph(f, utf8Next(s)); if (g) pen4 += g->adv4; }
        return (pen4 + 2) / 4;
    }
    while (*s) {
        const GfxGlyph* g = findGlyph(f, utf8Next(s));
        if (!g) continue;
        int gx = ((pen4 + 2) >> 2) + g->xoff;
        int gy = y + g->yoff;
        int x0, y0, x1, y1;
        if (g->w && clipBox(gx, gy, gx + g->w, gy + g->h, x0, y0, x1, y1)) {
            for (int yy = y0; yy < y1; yy++) {
                const uint8_t* src = f.bits + g->off + (yy - gy) * g->w + (x0 - gx);
                for (int xx = x0; xx < x1; xx++) plot(xx, yy, c, *src++);
            }
        }
        pen4 += g->adv4;
    }
    return (pen4 + 2) / 4;
}
