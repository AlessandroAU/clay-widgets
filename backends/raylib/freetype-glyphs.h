// freetype-glyphs.h - hinted glyph rasterization for the raylib backend.
//
// raylib rasterizes TTFs through stb_truetype, which ignores hinting: stems and
// the x-height land wherever the outline falls, so at UI sizes a vertical stroke
// straddles two pixels as a grey pair and the tops of lowercase letters sit on a
// half-covered row. Whether a given size looks crisp or soft is luck. FreeType's
// light hinting snaps outlines to the pixel grid vertically only - the way
// DirectWrite renders UI text - which fixes both without distorting glyph shapes.
//
// Opt in by defining CLAY_WIDGETS_FREETYPE and linking FreeType (the Makefile
// builds it from subprojects/freetype). ClayWidgets_BakeFont then takes its
// glyphs from here instead of raylib's LoadFontData; the atlas, gamma curve and
// FontCache are unchanged. CLAY_WIDGETS_FREETYPE_LOAD_FLAGS picks the hinting
// (FT_LOAD_TARGET_NORMAL runs the font's own instructions, FT_LOAD_NO_HINTING
// reproduces stb's look for comparison).

#ifndef CLAY_WIDGETS_FREETYPE_GLYPHS_H
#define CLAY_WIDGETS_FREETYPE_GLYPHS_H

#include <ft2build.h>
#include FT_FREETYPE_H

#include "raylib.h"

#ifndef CLAY_WIDGETS_FREETYPE_LOAD_FLAGS
#define CLAY_WIDGETS_FREETYPE_LOAD_FLAGS FT_LOAD_TARGET_LIGHT
#endif

// One library instance for the process, created on first bake. Faces are
// short-lived (one per bake), so there is nothing else to keep around.
static FT_Library ClayWidgets_FreeTypeLibrary() {
    static FT_Library library = nullptr;
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        if (FT_Init_FreeType(&library) != 0) {
            library = nullptr;
            TraceLog(LOG_WARNING, "clay-widgets: FreeType failed to initialise, falling back to stb_truetype");
        }
    }
    return library;
}

// Fills raylib GlyphInfo records for the codepoints the font has, in the same
// shape LoadFontData returns: 8-bit coverage images, offsets from the top of a
// pixelSize-tall line box, whole-pixel advances. Codepoints missing from the
// font are skipped, as raylib does, so FontCache's fallback lookup still works.
// Returns null (and *glyphCount 0) on failure; free with UnloadFontData.
static GlyphInfo *ClayWidgets_LoadGlyphsFreeType(const unsigned char *fontData, int fontDataSize, int pixelSize,
    const int *codepoints, int codepointCount, int *glyphCount) {
    *glyphCount = 0;
    FT_Library library = ClayWidgets_FreeTypeLibrary();
    FT_Face face = nullptr;
    if (!library || FT_New_Memory_Face(library, fontData, fontDataSize, 0, &face) != 0) {
        return nullptr;
    }

    // raylib sizes a font by its ascender-to-descender height, not its em, so
    // "16" means the same thing here as on the stb path and layouts keep their
    // proportions whichever rasterizer is compiled in.
    FT_Size_RequestRec request = {};
    request.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
    request.height = (FT_Long)pixelSize*64;
    if (FT_Request_Size(face, &request) != 0) {
        FT_Done_Face(face);
        return nullptr;
    }
    // Baseline, in whole pixels from the top of the line box.
    const int baseline = (int)((FT_MulFix(face->ascender, face->size->metrics.y_scale) + 32) >> 6);

    GlyphInfo *glyphs = (GlyphInfo *)MemAlloc((unsigned int)(codepointCount*sizeof(GlyphInfo)));
    if (!glyphs) {
        FT_Done_Face(face);
        return nullptr;
    }
    int count = 0;
    for (int i = 0; i < codepointCount; i++) {
        const int cp = codepoints[i];
        const FT_UInt index = FT_Get_Char_Index(face, (FT_ULong)cp);
        // Embedded bitmap strikes skip the hinter and would not match the
        // outlines drawn at every other size, so always render the outline.
        if (index == 0 || FT_Load_Glyph(face, index, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP
                | CLAY_WIDGETS_FREETYPE_LOAD_FLAGS) != 0) {
            continue;
        }
        const FT_GlyphSlot slot = face->glyph;
        const FT_Bitmap &bitmap = slot->bitmap;
        const bool gray = bitmap.pixel_mode == FT_PIXEL_MODE_GRAY;
        if (bitmap.rows > 0 && !gray && bitmap.pixel_mode != FT_PIXEL_MODE_MONO) {
            continue; // Colour or LCD output: not something this atlas can hold.
        }

        GlyphInfo &glyph = glyphs[count];
        glyph.value = cp;
        glyph.offsetX = slot->bitmap_left;
        glyph.offsetY = baseline - slot->bitmap_top;
        // Hinted advances are already whole pixels; rounding (rather than
        // raylib's truncation) keeps spacing even along a run.
        glyph.advanceX = (int)((slot->advance.x + 32) >> 6);
        glyph.image.width = (int)bitmap.width;
        glyph.image.height = (int)bitmap.rows;
        glyph.image.mipmaps = 1;
        glyph.image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        glyph.image.data = nullptr;
        if (bitmap.width > 0 && bitmap.rows > 0) {
            unsigned char *pixels = (unsigned char *)MemAlloc(bitmap.width*bitmap.rows);
            if (!pixels) {
                continue;
            }
            // A negative pitch stores rows bottom-up.
            const int pitch = bitmap.pitch;
            const unsigned char *top = pitch >= 0 ? bitmap.buffer
                                                  : bitmap.buffer + (bitmap.rows - 1)*(unsigned int)(-pitch);
            for (unsigned int y = 0; y < bitmap.rows; y++) {
                const unsigned char *row = top + (long)y*pitch;
                unsigned char *out = pixels + y*bitmap.width;
                for (unsigned int x = 0; x < bitmap.width; x++) {
                    out[x] = gray ? row[x] : ((row[x >> 3] & (0x80 >> (x & 7))) ? 255 : 0);
                }
            }
            glyph.image.data = pixels;
        }
        count++;
    }
    FT_Done_Face(face);

    if (count == 0) {
        MemFree(glyphs);
        return nullptr;
    }
    *glyphCount = count;
    return glyphs;
}

#endif // CLAY_WIDGETS_FREETYPE_GLYPHS_H
