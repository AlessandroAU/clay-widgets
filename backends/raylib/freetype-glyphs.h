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
#include FT_MULTIPLE_MASTERS_H

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

// Opens faceIndex's face, at its named instance if it has one. FreeType 2.14.3
// and earlier skip a variable font's HVAR advance deltas for a face opened as a
// named instance (fixed upstream in 5178bdc2a, not yet released), so a bold
// instance gets bold outlines on regular spacing and letters crowd. Opening the
// plain face and setting the instance's coordinates makes it a variation, which
// those versions do adjust; the outlines are the same either way.
static FT_Face ClayWidgets_OpenFace(FT_Library library, const unsigned char *fontData, int fontDataSize,
    long faceIndex) {
    FT_Face face = nullptr;
    if (FT_New_Memory_Face(library, fontData, fontDataSize, (FT_Long)(faceIndex & 0xFFFF), &face) != 0) {
        return nullptr;
    }
    const long instance = faceIndex >> 16;
    if (instance == 0) {
        return face;
    }
    FT_MM_Var *variation = nullptr;
    bool applied = false;
    if (FT_Get_MM_Var(face, &variation) == 0) {
        applied = instance <= (long)variation->num_namedstyles &&
            FT_Set_Var_Design_Coordinates(face, variation->num_axis, variation->namedstyle[instance - 1].coords) == 0;
        FT_Done_MM_Var(library, variation);
    }
    if (!applied) {
        FT_Done_Face(face);
        return nullptr;
    }
    return face;
}

// Fills raylib GlyphInfo records for the codepoints the font has, in the same
// shape LoadFontData returns: 8-bit coverage images, offsets from the top of a
// pixelSize-tall line box, whole-pixel advances. Codepoints missing from the
// font are skipped, as raylib does, so FontCache's fallback lookup still works.
// faceIndex is FreeType's: the face within a collection (.ttc/.otc) in the low
// 16 bits, and a variable font's named instance, counted from 1, in the bits
// above - the encoding fontconfig reports as FC_INDEX. Variable fonts such as
// Cantarell and Inter ship their bold as an instance of one file, so this is
// how to reach it; 0 is the first face at its default instance.
// Returns null (and *glyphCount 0) on failure; free with UnloadFontData.
static GlyphInfo *ClayWidgets_LoadGlyphsFreeType(const unsigned char *fontData, int fontDataSize, int pixelSize,
    const int *codepoints, int codepointCount, int *glyphCount, long faceIndex = 0) {
    *glyphCount = 0;
    FT_Library library = ClayWidgets_FreeTypeLibrary();
    FT_Face face = library ? ClayWidgets_OpenFace(library, fontData, fontDataSize, faceIndex) : nullptr;
    if (!face) {
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
