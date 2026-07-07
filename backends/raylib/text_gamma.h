// text_gamma.h - optional gamma-correct font-atlas baking for the raylib text
// path.
//
// raylib composites glyph coverage in gamma (sRGB) space rather than linear
// light, so the partial-coverage pixels along anti-aliased edges come out too
// light and text reads thin - most visibly as light text on dark backgrounds.
// Baking the atlas coverage through a 1/gamma curve approximates compositing in
// linear light and restores stem weight. This is a CPU-side fix (no shader), so
// it works identically on desktop and WebAssembly.
//
// Usage: define CLAY_WIDGETS_TEXT_GAMMA_CORRECTION before including this header
// to enable it; leave it undefined for raylib's stock bake. Optionally override
// the curve with CLAY_WIDGETS_TEXT_GAMMA (default 1.8f; 2.2f is full
// linearization and can look heavy, 1.0f is a no-op). Either way, call
// ClayWidgets_BakeFont() wherever you would call LoadFontFromMemory().

#ifndef CLAY_WIDGETS_TEXT_GAMMA_H
#define CLAY_WIDGETS_TEXT_GAMMA_H

#include "raylib.h"

// Bakes a .ttf byte array at pixelSize into a raylib Font. With correction
// enabled the atlas coverage is gamma-corrected before upload; otherwise this
// is a thin wrapper over LoadFontFromMemory. Returns a Font whose texture.id is
// 0 on failure. Free the result with UnloadFont().
static Font ClayWidgets_BakeFont(const unsigned char *fontData, int fontDataSize, int pixelSize);

#if defined(CLAY_WIDGETS_TEXT_GAMMA_CORRECTION)

#ifndef CLAY_WIDGETS_TEXT_GAMMA
#define CLAY_WIDGETS_TEXT_GAMMA 1.6f
#endif

#include <math.h>

static Font ClayWidgets_BakeFont(const unsigned char *fontData, int fontDataSize, int pixelSize) {
    // Mirror LoadFontFromMemory's pipeline (LoadFontData -> GenImageFontAtlas)
    // so we can reach the atlas image and correct it before it goes to the GPU.
    int glyphCount = 0;
    GlyphInfo *glyphs = LoadFontData(fontData, fontDataSize, pixelSize, NULL, 95, FONT_DEFAULT, &glyphCount);
    if (glyphs == NULL || glyphCount <= 0) {
        Font empty = {};
        return empty;
    }

    const int padding = 4; // FONT_TTF_DEFAULT_CHARS_PADDING; must match font.glyphPadding
    Rectangle *recs = NULL;
    Image atlas = GenImageFontAtlas(glyphs, &recs, glyphCount, pixelSize, padding, 0);

    // GenImageFontAtlas yields GRAY_ALPHA: 2 bytes/texel [gray=255, coverage].
    // Correct only the coverage byte, via a 256-entry LUT (one powf per level).
    const float gamma = (float)(CLAY_WIDGETS_TEXT_GAMMA);
    if (atlas.data != NULL && atlas.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA
        && gamma > 0.0f && gamma != 1.0f) {
        unsigned char lut[256];
        for (int i = 0; i < 256; i++) {
            float corrected = powf((float)i / 255.0f, 1.0f / gamma);
            int v = (int)(corrected * 255.0f + 0.5f);
            lut[i] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
        unsigned char *px = (unsigned char *)atlas.data;
        int texels = atlas.width * atlas.height;
        for (int i = 0; i < texels; i++) {
            px[i * 2 + 1] = lut[px[i * 2 + 1]];
        }
    }

    Font font = {};
    font.baseSize = pixelSize;
    font.glyphCount = glyphCount;
    font.glyphPadding = padding;
    font.texture = LoadTextureFromImage(atlas);
    font.recs = recs;
    font.glyphs = glyphs;
    UnloadImage(atlas);
    return font;
}

#else // correction disabled - stock raylib bake

static Font ClayWidgets_BakeFont(const unsigned char *fontData, int fontDataSize, int pixelSize) {
    return LoadFontFromMemory(".ttf", fontData, fontDataSize, pixelSize, NULL, 0);
}

#endif // CLAY_WIDGETS_TEXT_GAMMA_CORRECTION

#endif // CLAY_WIDGETS_TEXT_GAMMA_H
