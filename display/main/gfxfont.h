#pragma once
#include <stdint.h>

// Adafruit GFX font structs, PROGMEM removed for ESP-IDF.

typedef struct {
    uint16_t bitmapOffset;  // offset into font bitmap array
    uint8_t  width;         // glyph bitmap width  (px)
    uint8_t  height;        // glyph bitmap height (px)
    uint8_t  xAdvance;      // cursor advance after this glyph (px)
    int8_t   xOffset;       // x offset from cursor to bitmap UL corner
    int8_t   yOffset;       // y offset from cursor to bitmap UL corner
} GFXglyph;

typedef struct {
    uint8_t  *bitmap;       // packed glyph bitmaps, MSB first, no row padding
    GFXglyph *glyph;        // per-glyph metrics array
    uint16_t  first;        // ASCII code of first glyph
    uint16_t  last;         // ASCII code of last glyph
    uint8_t   yAdvance;     // line-height (px)
} GFXfont;
