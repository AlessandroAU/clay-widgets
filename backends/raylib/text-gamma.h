// text-gamma.h - font-atlas baking for the raylib text path, with optional
// gamma correction and optional FreeType hinting.
//
// raylib composites glyph coverage in gamma (sRGB) space rather than linear
// light, so the partial-coverage pixels along anti-aliased edges come out too
// light and text reads thin - most visibly as light text on dark backgrounds.
// Baking the atlas coverage through a 1/gamma curve approximates compositing in
// linear light and restores stem weight. This is a CPU-side fix (no shader), so
// it works identically on desktop and WebAssembly.
//
// The curve has to follow the text's polarity. Light glyphs on a dark
// background come out too thin and want an exponent above 1; dark glyphs on a
// light background come out too heavy and want its reciprocal. One baked atlas
// therefore cannot serve both themes, so the curve is a per-bake argument
// rather than a compile-time constant.
//
// Usage: define CLAY_WIDGETS_TEXT_GAMMA_CORRECTION before including this header
// to enable the curve; leave it undefined to keep raw coverage.
// CLAY_WIDGETS_TEXT_GAMMA (default 1.6f) is the light-on-dark default; 2.2f is
// full linearization and can look heavy, 1.0f is a no-op. Define
// CLAY_WIDGETS_FREETYPE (and link FreeType) to rasterize hinted glyphs instead
// of raylib's unhinted stb_truetype ones - see freetype-glyphs.h. Either way,
// call ClayWidgets_BakeFont() wherever you would call LoadFontFromMemory().

#ifndef CLAY_WIDGETS_TEXT_GAMMA_H
#define CLAY_WIDGETS_TEXT_GAMMA_H

#include <math.h>

#include "raylib.h"

#ifdef CLAY_WIDGETS_FREETYPE
#include "backends/raylib/freetype-glyphs.h"
#endif

// Default curve for light glyphs on a dark background; ClayWidgets_BakeFont
// takes the reciprocal for dark-on-light.
#ifndef CLAY_WIDGETS_TEXT_GAMMA
#define CLAY_WIDGETS_TEXT_GAMMA 1.6f
#endif

// Bakes a .ttf byte array at pixelSize into a raylib Font. With correction
// enabled the atlas coverage is raised to 1/gamma before upload. Returns a Font
// whose texture.id is 0 on failure. Free the result with UnloadFont().
static Font ClayWidgets_BakeFont(const unsigned char *fontData, int fontDataSize, int pixelSize,
    const int *codepoints = nullptr, int codepointCount = 0, float gamma = CLAY_WIDGETS_TEXT_GAMMA) {
    // Mirror LoadFontFromMemory's pipeline (LoadFontData -> GenImageFontAtlas)
    // so we can choose the rasterizer and reach the atlas image before it goes
    // to the GPU.
    int glyphCount = 0;
    int latin1[191];
    if (!codepoints || codepointCount <= 0) {
        for (int i = 0; i < 95; ++i) latin1[i] = i + 32;
        for (int i = 95; i < 191; ++i) latin1[i] = i - 95 + 160;
        codepoints = latin1; codepointCount = 191;
    }
    GlyphInfo *glyphs = NULL;
#ifdef CLAY_WIDGETS_FREETYPE
    glyphs = ClayWidgets_LoadGlyphsFreeType(fontData, fontDataSize, pixelSize, codepoints, codepointCount, &glyphCount);
#endif
    if (glyphs == NULL) {
        glyphs = LoadFontData(fontData, fontDataSize, pixelSize, codepoints, codepointCount, FONT_DEFAULT, &glyphCount);
    }
    if (glyphs == NULL || glyphCount <= 0) {
        Font empty = {};
        return empty;
    }

    // GenImageFontAtlas spaces its rows by the size it is given, and glyphs are
    // drawn with their padding included, so a glyph taller than that size would
    // bleed into the row below. stb's outlines stay within it, but hinting can
    // round one up a pixel (Roboto at 8 and 10 px), so size rows by the tallest.
    int rowHeight = pixelSize;
    for (int i = 0; i < glyphCount; i++) {
        if (glyphs[i].image.height > rowHeight) rowHeight = glyphs[i].image.height;
    }
    const int padding = 4; // FONT_TTF_DEFAULT_CHARS_PADDING; must match font.glyphPadding
    Rectangle *recs = NULL;
    Image atlas = GenImageFontAtlas(glyphs, &recs, glyphCount, rowHeight, padding, 0);

#if defined(CLAY_WIDGETS_TEXT_GAMMA_CORRECTION)
    // GenImageFontAtlas yields GRAY_ALPHA: 2 bytes/texel [gray=255, coverage].
    // Correct only the coverage byte, via a 256-entry LUT (one powf per level).
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
#else
    (void)gamma;
#endif

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

#endif // CLAY_WIDGETS_TEXT_GAMMA_H
