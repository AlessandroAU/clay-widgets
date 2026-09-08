// Clay -> raylib rendering backend for clay-widgets.
//
// Turns a Clay_RenderCommandArray into raylib draw calls, plus the pieces the
// renderer needs around it: a per-pixel-size font-atlas cache (FontCache), the
// text-measurement callback Clay calls during layout, and the emscripten
// frame-loop glue for the web build. An app includes this once and calls
// RenderClayCommands() each frame; the widget library itself stays
// renderer-agnostic.
#ifndef CLAY_WIDGETS_RAYLIB_RENDERER_H
#define CLAY_WIDGETS_RAYLIB_RENDERER_H

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "clay.h"
#include "clay-widgets/widgets.h"
#include "raylib.h"

// Gamma-correct the text atlas so anti-aliased edges aren't composited too light
// (raylib blends coverage in sRGB space, which makes text read thin - worst on
// the dark themes). CPU-side, not a shader, so it also works on web. Comment out
// the define to disable, or tune the curve with e.g. -DCLAY_WIDGETS_TEXT_GAMMA=2.2f.
#ifndef CLAY_WIDGETS_DISABLE_TEXT_GAMMA
#define CLAY_WIDGETS_TEXT_GAMMA_CORRECTION
#endif
#include "backends/raylib/text-gamma.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <functional>
#endif

namespace {

#ifdef __EMSCRIPTEN__
// The browser owns the frame loop, so on web we hand a per-frame callback to
// emscripten instead of spinning our own while() loop. The callback captures
// main()'s locals by reference; compiling with -sASYNCIFY and
// simulate_infinite_loop=1 keeps main()'s stack (and therefore those locals)
// alive for the lifetime of the page.
static std::function<void()> g_webFrame;
static void ClayWidgets_WebFrame() {
    if (g_webFrame) {
        g_webFrame();
    }
}

// Resize the raylib window (and thus the canvas) to match the browser window.
// raylib reads GetScreenWidth()/GetScreenHeight() every frame, so this is all
// the Clay layout needs to reflow responsively. Emscripten's GLFW handles the
// HiDPI backing-store scaling, so text stays crisp.
static void ClayWidgets_SyncCanvasToWindow() {
    int w = EM_ASM_INT({ return window.innerWidth; });
    int h = EM_ASM_INT({ return window.innerHeight; });
    if (w > 0 && h > 0) {
        SetWindowSize(w, h);
    }
}

static EM_BOOL ClayWidgets_OnResize(int, const EmscriptenUiEvent *e, void *) {
    if (e && e->windowInnerWidth > 0 && e->windowInnerHeight > 0) {
        SetWindowSize(e->windowInnerWidth, e->windowInnerHeight);
    } else {
        ClayWidgets_SyncCanvasToWindow();
    }
    return EM_FALSE;
}
#endif

static Color ToRaylibColor(Clay_Color c) {
    auto clampByte = [](float v) -> unsigned char {
        if (v < 0.0f) return 0;
        if (v > 255.0f) return 255;
        return static_cast<unsigned char>(std::round(v));
    };
    return Color{clampByte(c.r), clampByte(c.g), clampByte(c.b), clampByte(c.a)};
}

static Color MixOverlay(Color base, Color overlay) {
    float a = static_cast<float>(overlay.a) / 255.0f;
    Color out = {
        static_cast<unsigned char>(base.r + (overlay.r - base.r) * a),
        static_cast<unsigned char>(base.g + (overlay.g - base.g) * a),
        static_cast<unsigned char>(base.b + (overlay.b - base.b) * a),
        base.a,
    };
    return out;
}

static void AppendUtf8FromCodepoint(char *buffer, int &length, int maxLength, int codepoint) {
    int bytes = codepoint <= 0x7F ? 1 : codepoint <= 0x7FF ? 2 : codepoint <= 0xFFFF ? 3 : 4;
    if (!buffer || length < 0 || codepoint < 0 || codepoint > 0x10FFFF
        || (codepoint >= 0xD800 && codepoint <= 0xDFFF) || maxLength - length < bytes) {
        return;
    }

    auto pushByte = [&](unsigned char b) {
        if (length < maxLength) {
            buffer[length++] = static_cast<char>(b);
        }
    };

    if (codepoint <= 0x7F) {
        pushByte(static_cast<unsigned char>(codepoint));
        return;
    }
    if (codepoint <= 0x7FF) {
        pushByte(static_cast<unsigned char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if (codepoint <= 0xFFFF) {
        pushByte(static_cast<unsigned char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F)));
        pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    pushByte(static_cast<unsigned char>(0xF0 | ((codepoint >> 18) & 0x07)));
    pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 12) & 0x3F)));
    pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F)));
    pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
}

// A raylib Font bakes every glyph into one texture at a single pixel size; then
// DrawTextEx scales that bitmap to whatever size you draw - and scaling a baked
// bitmap is what softens text. FontCache instead bakes one atlas per pixel size
// the UI actually asks for, lazily, so every glyph is drawn ~1:1. The kit uses
// only a handful of sizes (13/14/16/18/24/30), so the cache stays tiny.
//
// Sizes are keyed in *physical* pixels: raylib's HighDPI path scales every draw
// by GetWindowScaleDPI(), so a glyph drawn at logical size S covers S*dpi device
// pixels. Baking at round(S*dpi) and drawing with the logical size S makes the
// atlas sample 1:1 on screen at any DPI; measurement stays in logical units
// because MeasureTextEx divides the same factor back out.
//
// The TTF bytes are supplied by the app (ttfData/ttfSize) so this backend has
// no opinion about which font ships - the demo points them at its embedded
// font header; another app can point them at any TTF in memory.
struct FontFace {
    uint16_t id;
    const unsigned char *data;
    int size;
    std::vector<int> codepoints;
    std::vector<std::pair<int, Font>> atlases;
};
struct FontCache {
    float dpiScale = 1.0f;
    Font fallback = {};                        // used if the app font failed
    const unsigned char *ttfData = nullptr;    // app-supplied TTF bytes
    int ttfSize = 0;
    bool haveEmbedded = false;                 // true once a bake from ttfData succeeded
    std::vector<std::pair<int, Font>> atlases; // physical pixel size -> baked Font
    std::vector<FontFace> faces; // registered IDs; registration order is fallback order
};

// TTF bytes must outlive the cache. Glyph lists are copied. Register before layout;
// call Clay_ResetMeasureTextCache after replacing a face used by existing text.
[[maybe_unused]] static bool FontCache_Register(FontCache &cache, uint16_t id, const unsigned char *data, int size,
    const int *codepoints = nullptr, int count = 0) {
    if (!data || size <= 0 || count < 0 || (count && !codepoints)) return false;
    for (auto &face : cache.faces) if (face.id == id) {
        for (auto &atlas : face.atlases) UnloadFont(atlas.second);
        face = FontFace{id, data, size, {}, {}};
        if (count) face.codepoints.assign(codepoints, codepoints + count);
        return true;
    }
    cache.faces.push_back(FontFace{id, data, size, {}, {}});
    if (count) cache.faces.back().codepoints.assign(codepoints, codepoints + count);
    return true;
}

// Maps a logical font size to the physical pixel size we bake at (min 1).
static int FontCache_PixelSize(const FontCache &cache, float logicalSize) {
    int px = static_cast<int>(std::lround(logicalSize * cache.dpiScale));
    return px < 1 ? 1 : px;
}

// Returns the atlas baked at pixelSize, baking and caching it on first use.
// Never returns null: falls back to the default font if no app font was
// supplied or a bake fails.
static Font *FontCache_Get(FontCache &cache, int pixelSize, uint16_t fontId = 0) {
    for (auto &face : cache.faces) if (face.id == fontId) {
        for (auto &atlas : face.atlases) if (atlas.first == pixelSize) return &atlas.second;
        Font font = ClayWidgets_BakeFont(face.data, face.size, pixelSize, face.codepoints.data(), (int)face.codepoints.size());
        if (!font.texture.id) return &cache.fallback;
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
        face.atlases.emplace_back(pixelSize, font);
        return &face.atlases.back().second;
    }
    if (!cache.haveEmbedded || !cache.ttfData || cache.ttfSize <= 0) {
        return &cache.fallback;
    }
    for (auto &entry : cache.atlases) {
        if (entry.first == pixelSize) {
            return &entry.second;
        }
    }
    Font baked = ClayWidgets_BakeFont(cache.ttfData, cache.ttfSize, pixelSize);
    if (baked.texture.id == 0) {
        return &cache.fallback;
    }
    SetTextureFilter(baked.texture, TEXTURE_FILTER_BILINEAR);
    cache.atlases.emplace_back(pixelSize, baked);
    return &cache.atlases.back().second;
}

static void FontCache_Unload(FontCache &cache) {
    for (auto &face : cache.faces) for (auto &atlas : face.atlases) UnloadFont(atlas.second);
    cache.faces.clear();
    for (auto &entry : cache.atlases) {
        UnloadFont(entry.second);
    }
    cache.atlases.clear();
    cache.haveEmbedded = false;
}

// raylib's text APIs want NUL-terminated strings while Clay hands out slices,
// so measure/draw stage through one reusable buffer instead of allocating a
// std::string per call (there is one call per text run per frame). The UI is
// single-threaded, as is raylib itself.
static const char *TerminateSlice(const char *chars, int32_t length) {
    static std::string buffer;
    buffer.assign(chars, static_cast<size_t>(length));
    return buffer.c_str();
}

static Font FontCache_Glyph(FontCache &cache, uint16_t id, float size, int codepoint) {
    int px = FontCache_PixelSize(cache, size);
    Font primary = *FontCache_Get(cache, px, id);
    if (primary.glyphs[GetGlyphIndex(primary, codepoint)].value == codepoint) return primary;
    for (auto &face : cache.faces) {
        if (face.id == id) continue;
        Font fallback = *FontCache_Get(cache, px, face.id);
        if (fallback.glyphs[GetGlyphIndex(fallback, codepoint)].value == codepoint) return fallback;
    }
    return primary;
}

// Measurement and rendering deliberately share glyph fallback and advances.
static Clay_Dimensions FontCache_Text(FontCache &cache, uint16_t id, const char *chars, int length,
    float size, float spacing, Vector2 origin, Color color, bool draw) {
    const char *text = TerminateSlice(chars, length);
    float x = 0, y = 0, width = 0;
    for (int offset = 0; offset < length;) {
        int bytes = 1; int cp = GetCodepointNext(text + offset, &bytes);
        offset += bytes;
        if (cp == '\n') { width = std::max(width, x > 0 ? x - spacing : 0); x = 0; y += size; continue; }
        Font font = FontCache_Glyph(cache, id, size, cp);
        if (draw) DrawTextCodepoint(font, cp, {origin.x + x, origin.y + y}, size, color);
        int index = GetGlyphIndex(font, cp);
        float advance = font.glyphs[index].advanceX ? (float)font.glyphs[index].advanceX : font.recs[index].width;
        x += advance * size / (float)font.baseSize + spacing;
    }
    return {std::max(width, x > 0 ? x - spacing : 0), y + size};
}

static Clay_Dimensions MeasureTextRaylib(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    FontCache *cache = static_cast<FontCache *>(userData);
    float fontSize = static_cast<float>(config->fontSize);
    if (cache) return FontCache_Text(*cache, config->fontId, text.chars, text.length, fontSize,
        (float)config->letterSpacing, {0,0}, WHITE, false);
    Font font = cache ? *FontCache_Get(*cache, FontCache_PixelSize(*cache, fontSize))
                      : GetFontDefault();

    float spacing = static_cast<float>(config->letterSpacing);
    Vector2 measured = MeasureTextEx(font, TerminateSlice(text.chars, text.length), fontSize, spacing);

    Clay_Dimensions out = {};
    out.width = measured.x;
    out.height = measured.y;
    return out;
}

static void HandleClayError(Clay_ErrorData errorData) {
    std::string msg(errorData.errorText.chars, errorData.errorText.length);
    TraceLog(LOG_ERROR, "Clay error (%d): %s", static_cast<int>(errorData.errorType), msg.c_str());
}

static Rectangle IntersectRects(Rectangle a, Rectangle b) {
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.width, b.x + b.width);
    float y2 = std::min(a.y + a.height, b.y + b.height);
    if (x2 < x1 || y2 < y1) {
        return Rectangle{x1, y1, 0, 0};
    }
    return Rectangle{x1, y1, x2 - x1, y2 - y1};
}

// Raylib's DrawRectangleRounded applies one radius to all four corners, so
// mixed-radius fills (e.g. the end cells of a segmented control, which round
// only their outer corners) are assembled from quarter-disc sectors plus
// non-overlapping horizontal strips - overlap would double-blend translucent
// fills.
static void DrawRectangleRoundedPerCorner(Rectangle rect, Clay_CornerRadius cr, int segments, Color color) {
    float maxRadius = 0.5f * std::min(rect.width, rect.height);
    float tl = std::min(cr.topLeft, maxRadius);
    float tr = std::min(cr.topRight, maxRadius);
    float bl = std::min(cr.bottomLeft, maxRadius);
    float br = std::min(cr.bottomRight, maxRadius);

    if (tl == tr && tl == bl && tl == br) {
        float minDim = std::max(1.0f, std::min(rect.width, rect.height));
        float roundness = std::min(1.0f, 2.0f * tl / minDim);
        DrawRectangleRounded(rect, roundness, segments, color);
        return;
    }

    if (tl > 0.0f) DrawCircleSector(Vector2{rect.x + tl, rect.y + tl}, tl, 180.0f, 270.0f, segments, color);
    if (tr > 0.0f) DrawCircleSector(Vector2{rect.x + rect.width - tr, rect.y + tr}, tr, 270.0f, 360.0f, segments, color);
    if (br > 0.0f) DrawCircleSector(Vector2{rect.x + rect.width - br, rect.y + rect.height - br}, br, 0.0f, 90.0f, segments, color);
    if (bl > 0.0f) DrawCircleSector(Vector2{rect.x + bl, rect.y + rect.height - bl}, bl, 90.0f, 180.0f, segments, color);

    // The rest is the rect minus the four corner squares: split it at every
    // corner-square edge into horizontal strips, insetting each strip past
    // whichever corner squares it runs alongside.
    float ys[6] = {0.0f, tl, tr, rect.height - bl, rect.height - br, rect.height};
    std::sort(ys, ys + 6);
    for (int s = 0; s < 5; ++s) {
        float y1 = ys[s];
        float y2 = ys[s + 1];
        if (y2 - y1 <= 0.0f) {
            continue;
        }
        float mid = 0.5f * (y1 + y2);
        float left = (mid < tl) ? tl : ((mid > rect.height - bl) ? bl : 0.0f);
        float right = (mid < tr) ? tr : ((mid > rect.height - br) ? br : 0.0f);
        DrawRectangleRec(Rectangle{rect.x + left, rect.y + y1, rect.width - left - right, y2 - y1}, color);
    }
}

static void RenderClayCommands(Clay_RenderCommandArray commands, FontCache &fontCache) {
    std::vector<Rectangle> scissorStack;
    std::vector<Color> overlayStack;
    bool scissorEnabled = false;
    const int roundedCornerSegments = 24;

    auto reapplyScissor = [&]() {
        if (scissorEnabled) {
            EndScissorMode();
            scissorEnabled = false;
        }
        if (!scissorStack.empty()) {
            Rectangle r = scissorStack.back();
            BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height));
            scissorEnabled = true;
        }
    };

    auto applyOverlay = [&](Color c) -> Color {
        Color out = c;
        for (Color overlay : overlayStack) {
            out = MixOverlay(out, overlay);
        }
        return out;
    };

    for (int i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        // Snap boxes to whole pixels. Clay lays out in floats, so sibling
        // edges land on fractional coordinates and rasterize as a half-pixel
        // blur that reads as uneven widths and gaps (three equally-grown
        // cards can each look a different size). Rounding the two edges
        // independently (rather than x and width) keeps adjacent elements
        // flush with each other.
        float x0 = std::round(cmd->boundingBox.x);
        float y0 = std::round(cmd->boundingBox.y);
        Rectangle rect = {
            x0,
            y0,
            std::round(cmd->boundingBox.x + cmd->boundingBox.width) - x0,
            std::round(cmd->boundingBox.y + cmd->boundingBox.height) - y0,
        };

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Color color = applyOverlay(ToRaylibColor(cmd->renderData.rectangle.backgroundColor));
                DrawRectangleRoundedPerCorner(rect, cmd->renderData.rectangle.cornerRadius, roundedCornerSegments, color);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                const Clay_TextRenderData &text = cmd->renderData.text;
                Color color = applyOverlay(ToRaylibColor(text.textColor));
                FontCache_Text(fontCache, text.fontId, text.stringContents.chars, text.stringContents.length,
                    (float)text.fontSize, (float)text.letterSpacing, {rect.x, rect.y}, color, true);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                const Clay_BorderRenderData &b = cmd->renderData.border;
                Color c = applyOverlay(ToRaylibColor(b.color));
                float radius = b.cornerRadius.topLeft;
                bool uniformWidth = b.width.left == b.width.right
                    && b.width.top == b.width.bottom
                    && b.width.left == b.width.top;
                if (radius > 0.0f && uniformWidth && b.width.left > 0) {
                    // Draw a rounded, inset border whose outer edge aligns with the
                    // element's rounded background so fills never appear to bleed past it.
                    float t = static_cast<float>(b.width.left);
                    Rectangle inner = { rect.x + t, rect.y + t, rect.width - 2.0f * t, rect.height - 2.0f * t };
                    float minDim = std::min(inner.width, inner.height);
                    float innerRadius = std::max(0.0f, radius - t);
                    float roundness = minDim > 0.0f ? std::min(1.0f, 2.0f * innerRadius / minDim) : 0.0f;
                    DrawRectangleRoundedLinesEx(inner, roundness, roundedCornerSegments, t, c);
                } else {
                    if (b.width.top > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), static_cast<int>(rect.width), b.width.top, c);
                    if (b.width.bottom > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y + rect.height - b.width.bottom), static_cast<int>(rect.width), b.width.bottom, c);
                    if (b.width.left > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), b.width.left, static_cast<int>(rect.height), c);
                    if (b.width.right > 0) DrawRectangle(static_cast<int>(rect.x + rect.width - b.width.right), static_cast<int>(rect.y), b.width.right, static_cast<int>(rect.height), c);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                if (scissorStack.empty()) {
                    scissorStack.push_back(rect);
                } else {
                    scissorStack.push_back(IntersectRects(scissorStack.back(), rect));
                }
                reapplyScissor();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                if (!scissorStack.empty()) {
                    scissorStack.pop_back();
                }
                reapplyScissor();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START: {
                overlayStack.push_back(ToRaylibColor(cmd->renderData.overlayColor.color));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END: {
                if (!overlayStack.empty()) {
                    overlayStack.pop_back();
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                const Clay_ImageRenderData &img = cmd->renderData.image;
                if (img.imageData) {
                    Texture2D *tex = static_cast<Texture2D *>(img.imageData);
                    // The widget layer packs an optional 0xRRGGBBAA tint into
                    // userData (see clay-widgets/image.h). A null/zero value
                    // means untinted, so we draw the texture as-is (WHITE).
                    Color tint = WHITE;
                    if (cmd->userData) {
                        tint = ToRaylibColor(ClayWidgets_UnpackTint(cmd->userData));
                    }
                    tint = applyOverlay(tint);
                    Rectangle src = {0.0f, 0.0f, static_cast<float>(tex->width), static_cast<float>(tex->height)};
                    DrawTexturePro(*tex, src, rect, Vector2{0.0f, 0.0f}, 0.0f, tint);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            case CLAY_RENDER_COMMAND_TYPE_NONE:
            default:
                break;
        }
    }

    if (scissorEnabled) {
        EndScissorMode();
    }
}

} // namespace

#endif // CLAY_WIDGETS_RAYLIB_RENDERER_H
