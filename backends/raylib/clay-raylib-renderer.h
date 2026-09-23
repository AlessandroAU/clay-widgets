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
#include "rlgl.h"

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
//
// An atlas is identified by the size it was baked at and by the gamma curve
// baked into its coverage, because the correction that thickens light-on-dark
// text would thicken dark-on-light text too - the wrong way. Keying on both
// lets a light and a dark surface keep their own atlas instead of fighting
// over one.
struct BakedAtlas {
    int pixelSize;
    int gammaKey; // textGamma * 100, rounded (1/1.6 keys as 63); floats make poor map keys
    Font font;
};
struct FontFace {
    uint16_t id;
    const unsigned char *data;
    int size;
    std::vector<int> codepoints;
    std::vector<BakedAtlas> atlases;
};
struct FontCache {
    float dpiScale = 1.0f;
    float textGamma = CLAY_WIDGETS_TEXT_GAMMA; // > 1 thickens (light on dark), < 1 thins
    Font fallback = {};                        // used if the app font failed
    const unsigned char *ttfData = nullptr;    // app-supplied TTF bytes
    int ttfSize = 0;
    bool haveEmbedded = false;                 // true once a bake from ttfData succeeded
    std::vector<BakedAtlas> atlases;           // fallback face, keyed as above
    std::vector<FontFace> faces; // registered IDs; registration order is fallback order
};

static int FontCache_GammaKey(const FontCache &cache) {
    return (int)std::lround(cache.textGamma*100.0f);
}

// TTF bytes must outlive the cache. Glyph lists are copied. Register before layout;
// call Clay_ResetMeasureTextCache after replacing a face used by existing text.
[[maybe_unused]] static bool FontCache_Register(FontCache &cache, uint16_t id, const unsigned char *data, int size,
    const int *codepoints = nullptr, int count = 0) {
    if (!data || size <= 0 || count < 0 || (count && !codepoints)) return false;
    for (auto &face : cache.faces) if (face.id == id) {
        for (auto &atlas : face.atlases) UnloadFont(atlas.font);
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
    const int gammaKey = FontCache_GammaKey(cache);
    for (auto &face : cache.faces) if (face.id == fontId) {
        for (auto &atlas : face.atlases) {
            if (atlas.pixelSize == pixelSize && atlas.gammaKey == gammaKey) return &atlas.font;
        }
        Font font = ClayWidgets_BakeFont(face.data, face.size, pixelSize, face.codepoints.data(),
            (int)face.codepoints.size(), cache.textGamma);
        if (!font.texture.id) return &cache.fallback;
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
        face.atlases.push_back(BakedAtlas{pixelSize, gammaKey, font});
        return &face.atlases.back().font;
    }
    if (!cache.haveEmbedded || !cache.ttfData || cache.ttfSize <= 0) {
        return &cache.fallback;
    }
    for (auto &entry : cache.atlases) {
        if (entry.pixelSize == pixelSize && entry.gammaKey == gammaKey) {
            return &entry.font;
        }
    }
    Font baked = ClayWidgets_BakeFont(cache.ttfData, cache.ttfSize, pixelSize, nullptr, 0, cache.textGamma);
    if (baked.texture.id == 0) {
        return &cache.fallback;
    }
    SetTextureFilter(baked.texture, TEXTURE_FILTER_BILINEAR);
    cache.atlases.push_back(BakedAtlas{pixelSize, gammaKey, baked});
    return &cache.atlases.back().font;
}

static void FontCache_Unload(FontCache &cache) {
    for (auto &face : cache.faces) for (auto &atlas : face.atlases) UnloadFont(atlas.font);
    cache.faces.clear();
    for (auto &entry : cache.atlases) {
        UnloadFont(entry.font);
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
        // Draw the atlas at exactly one texel per device pixel. Fractional
        // origins and rounding logical sizes at fractional DPI otherwise blur
        // an already antialiased glyph through a second bilinear resampling.
        const float scale = cache.dpiScale > 0 ? cache.dpiScale : 1.f;
        const float rasterSize = (float)FontCache_PixelSize(cache, size) / scale;
        if (draw) DrawTextCodepoint(font, cp,
            {std::round((origin.x + x) * scale) / scale,
             std::round((origin.y + y) * scale) / scale}, rasterSize, color);
        int index = GetGlyphIndex(font, cp);
        float advance = font.glyphs[index].advanceX ? (float)font.glyphs[index].advanceX : font.recs[index].width;
        x += advance * rasterSize / (float)font.baseSize + spacing;
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

// Rounded boxes are drawn from a signed distance field rather than from
// triangle fans. Two reasons. raylib rasterizes DrawRectangleRounded with no
// multisampling, and the offscreen targets this backend renders into cannot be
// made multisampled, so every curve came out hard-stepped. And a background and
// its border arrive as two separate Clay commands: rasterizing them as two
// independent approximations of the same outline left the fill peeking out
// around the border as a speckled rim, most visible on a toggle's accent pill.
//
// One shader evaluates the rounded box once per fragment and derives both the
// fill and the ring from that single distance, so the two cannot disagree, and
// the coverage ramp antialiases every curve. The quad is measured in device
// pixels, which makes the distance gradient exactly one pixel per unit - no
// derivative instructions, so the shader stays valid on GLSL ES as well.

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
static const char *kClayWidgetsShapeVS = R"GLSL(#version 100
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec4 vertexColor;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform mat4 mvp;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
)GLSL";

static const char *kClayWidgetsShapeFS = R"GLSL(#version 100
precision highp float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform vec2 uQuadHalf;
uniform vec2 uRectHalf;
uniform vec4 uRadii;
uniform float uBorder;
uniform vec4 uFill;
uniform vec4 uStroke;

float ClayWidgets_BoxDistance(vec2 p, vec2 halfSize, vec4 radii) {
    float r = (p.y > 0.0) ? ((p.x > 0.0) ? radii.z : radii.w)
                          : ((p.x > 0.0) ? radii.y : radii.x);
    vec2 q = abs(p) - halfSize + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

void main() {
    vec2 p = (fragTexCoord*2.0 - 1.0)*uQuadHalf;
    float d = ClayWidgets_BoxDistance(p, uRectHalf, uRadii);
    float outer = clamp(0.5 - d, 0.0, 1.0);
    float inner = (uBorder > 0.0) ? clamp(0.5 - (d + uBorder), 0.0, 1.0) : outer;
    float ringAlpha = max(outer - inner, 0.0)*uStroke.a;
    float fillAlpha = inner*uFill.a;
    float alpha = ringAlpha + fillAlpha;
    vec3 rgb = (alpha > 0.0) ? (uStroke.rgb*ringAlpha + uFill.rgb*fillAlpha)/alpha : vec3(0.0);
    gl_FragColor = vec4(rgb, alpha)*fragColor;
}
)GLSL";
#else
static const char *kClayWidgetsShapeVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
out vec2 fragTexCoord;
out vec4 fragColor;
uniform mat4 mvp;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
)GLSL";

static const char *kClayWidgetsShapeFS = R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform vec2 uQuadHalf;   // half the drawn quad, device pixels (shape + AA margin)
uniform vec2 uRectHalf;   // half the shape itself, device pixels
uniform vec4 uRadii;      // radii in device pixels: top-left, top-right, bottom-right, bottom-left
uniform float uBorder;    // ring thickness, device pixels; 0 draws a solid fill
uniform vec4 uFill;       // straight-alpha fill color
uniform vec4 uStroke;     // straight-alpha border color

float ClayWidgets_BoxDistance(vec2 p, vec2 halfSize, vec4 radii) {
    float r = (p.y > 0.0) ? ((p.x > 0.0) ? radii.z : radii.w)
                          : ((p.x > 0.0) ? radii.y : radii.x);
    vec2 q = abs(p) - halfSize + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

void main() {
    // fragTexCoord spans the quad, so p lands in device pixels measured from the
    // shape's centre, with +y pointing down the screen.
    vec2 p = (fragTexCoord*2.0 - 1.0)*uQuadHalf;
    float d = ClayWidgets_BoxDistance(p, uRectHalf, uRadii);

    // A one-pixel linear ramp across each boundary approximates the pixel's area
    // coverage; p is in device pixels, so the distance gradient is exactly 1.
    float outer = clamp(0.5 - d, 0.0, 1.0);
    float inner = (uBorder > 0.0) ? clamp(0.5 - (d + uBorder), 0.0, 1.0) : outer;

    // Ring and fill tile the shape without overlapping, so the two premultiplied
    // contributions simply add; dividing back out keeps the output straight-alpha
    // like every other draw in this backend.
    float ringAlpha = max(outer - inner, 0.0)*uStroke.a;
    float fillAlpha = inner*uFill.a;
    float alpha = ringAlpha + fillAlpha;
    vec3 rgb = (alpha > 0.0) ? (uStroke.rgb*ringAlpha + uFill.rgb*fillAlpha)/alpha : vec3(0.0);
    finalColor = vec4(rgb, alpha)*fragColor;
}
)GLSL";
#endif

struct ClayWidgets_ShapeShader {
    Shader shader = {};
    bool attempted = false; // compile tried in the current GL context
    bool ready = false;
    int quadHalf = -1, rectHalf = -1, radii = -1, border = -1, fill = -1, stroke = -1;
};

// One shader per GL context, compiled on first use. ClayWidgets_UnloadShapes()
// resets it, attempt flag included, so a host that tears its context down and
// builds a new one compiles afresh instead of drawing through a stale program
// id or silently falling back to the aliased path.
static ClayWidgets_ShapeShader &ClayWidgets_Shapes() {
    static ClayWidgets_ShapeShader shapes;
    if (!shapes.attempted) {
        shapes.attempted = true;
        shapes.shader = LoadShaderFromMemory(kClayWidgetsShapeVS, kClayWidgetsShapeFS);
        if (IsShaderValid(shapes.shader)) {
            shapes.quadHalf = GetShaderLocation(shapes.shader, "uQuadHalf");
            shapes.rectHalf = GetShaderLocation(shapes.shader, "uRectHalf");
            shapes.radii = GetShaderLocation(shapes.shader, "uRadii");
            shapes.border = GetShaderLocation(shapes.shader, "uBorder");
            shapes.fill = GetShaderLocation(shapes.shader, "uFill");
            shapes.stroke = GetShaderLocation(shapes.shader, "uStroke");
            shapes.ready = shapes.quadHalf >= 0 && shapes.rectHalf >= 0 && shapes.radii >= 0
                && shapes.border >= 0 && shapes.fill >= 0 && shapes.stroke >= 0;
        }
        if (!shapes.ready) {
            TraceLog(LOG_WARNING, "clay-widgets: shape shader unavailable, shapes will be aliased");
        }
    }
    return shapes;
}

// Call before destroying the GL context the shader was compiled in.
[[maybe_unused]] static void ClayWidgets_UnloadShapes() {
    ClayWidgets_ShapeShader &shapes = ClayWidgets_Shapes();
    if (shapes.ready) {
        UnloadShader(shapes.shader);
    }
    shapes = ClayWidgets_ShapeShader{};
}

// Draws one rounded box from a single distance field: a ring of borderWidth
// inset from the outline in stroke, and the area inside that ring in fill. Pass
// BLANK to omit either, so a fill with a border width and BLANK stroke is a fill
// shrunk to the ring's inner edge. rect and the radii are in logical units;
// scale converts them to the device pixels the shader measures in. Returns
// false when the shader is unavailable so the caller can fall back.
static bool DrawRectangleRoundedSDF(Rectangle rect, Clay_CornerRadius cr, float borderWidth,
    Color fill, Color stroke, float scale) {
    ClayWidgets_ShapeShader &shapes = ClayWidgets_Shapes();
    if (!shapes.ready || rect.width <= 0.0f || rect.height <= 0.0f || scale <= 0.0f) {
        return false;
    }

    const float halfW = rect.width*0.5f*scale;
    const float halfH = rect.height*0.5f*scale;
    const float maxRadius = std::min(halfW, halfH);
    auto deviceRadius = [&](float r) { return std::min(std::max(r*scale, 0.0f), maxRadius); };
    float radii[4] = {deviceRadius(cr.topLeft), deviceRadius(cr.topRight),
                      deviceRadius(cr.bottomRight), deviceRadius(cr.bottomLeft)};
    float rectHalf[2] = {halfW, halfH};
    // The coverage ramp reaches one pixel past the outline, so the quad has to too.
    float quadHalf[2] = {halfW + 1.0f, halfH + 1.0f};
    float border = std::max(borderWidth*scale, 0.0f);
    auto toVec4 = [](Color c, float *out) {
        out[0] = c.r/255.0f; out[1] = c.g/255.0f; out[2] = c.b/255.0f; out[3] = c.a/255.0f;
    };
    float fillValue[4], strokeValue[4];
    toVec4(fill, fillValue);
    toVec4(stroke, strokeValue);

    // Begin/EndShaderMode bracket each shape because rlgl batches geometry: the
    // uniforms below describe this shape alone, and switching the active shader
    // is what forces the batch to reach the GPU before the next shape sets its own.
    BeginShaderMode(shapes.shader);
    SetShaderValue(shapes.shader, shapes.quadHalf, quadHalf, SHADER_UNIFORM_VEC2);
    SetShaderValue(shapes.shader, shapes.rectHalf, rectHalf, SHADER_UNIFORM_VEC2);
    SetShaderValue(shapes.shader, shapes.radii, radii, SHADER_UNIFORM_VEC4);
    SetShaderValue(shapes.shader, shapes.border, &border, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shapes.shader, shapes.fill, fillValue, SHADER_UNIFORM_VEC4);
    SetShaderValue(shapes.shader, shapes.stroke, strokeValue, SHADER_UNIFORM_VEC4);

    const float cx = rect.x + rect.width*0.5f;
    const float cy = rect.y + rect.height*0.5f;
    const float ex = quadHalf[0]/scale;
    const float ey = quadHalf[1]/scale;
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlColor4ub(255, 255, 255, 255);
        rlTexCoord2f(0.0f, 0.0f); rlVertex2f(cx - ex, cy - ey);
        rlTexCoord2f(0.0f, 1.0f); rlVertex2f(cx - ex, cy + ey);
        rlTexCoord2f(1.0f, 1.0f); rlVertex2f(cx + ex, cy + ey);
        rlTexCoord2f(1.0f, 0.0f); rlVertex2f(cx + ex, cy - ey);
    rlEnd();
    rlSetTexture(0);
    EndShaderMode();
    return true;
}

// Commands are in logical units and reach the device multiplied by
// fontCache.dpiScale, the same factor the font cache bakes its atlases at; the
// shape shader and the pixel snapping below both need it to reason in device
// pixels. On screen, raylib's HighDPI matrix applies that factor, scissor
// rectangles included. Pass offscreen = true when drawing into a render texture
// under your own rlScalef(dpiScale): raylib then takes scissor rectangles in raw
// texture pixels, so they are converted here.
static void RenderClayCommands(Clay_RenderCommandArray commands, FontCache &fontCache, bool offscreen = false) {
    const float scale = fontCache.dpiScale > 0.0f ? fontCache.dpiScale : 1.0f;
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
            if (offscreen) {
                // Round both edges, as the boxes are, so a clip stays flush with
                // the content it clips at fractional scales.
                BeginScissorMode(static_cast<int>(std::round(r.x * scale)), static_cast<int>(std::round(r.y * scale)),
                    static_cast<int>(std::round((r.x + r.width) * scale) - std::round(r.x * scale)),
                    static_cast<int>(std::round((r.y + r.height) * scale) - std::round(r.y * scale)));
            } else {
                BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                    static_cast<int>(r.height));
            }
            scissorEnabled = true;
        }
    };

    auto snapToDevice = [&](float v) {
        return scale > 0.0f ? std::round(v*scale)/scale : std::round(v);
    };

    // A fill and its border arrive as separate commands. Drawn to the same
    // outline, the fill's half-covered edge pixel is only half covered again by
    // the ring, leaving a faint rim of fill colour under an opaque border. So a
    // fill stops at the ring's inner edge when an opaque uniform border follows
    // it on the same box. Clay emits the border after the element's fill and
    // children, and border commands carry a derived id, so match on the box.
    struct OpaqueBorder {
        Clay_BoundingBox box;
        float width;
        int index;
    };
    std::vector<OpaqueBorder> opaqueBorders;
    for (int i = 0; i < commands.length; ++i) {
        const Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_BORDER) continue;
        const Clay_BorderRenderData &b = cmd->renderData.border;
        bool uniform = b.width.left > 0 && b.width.left == b.width.right
            && b.width.left == b.width.top && b.width.left == b.width.bottom;
        if (uniform && ToRaylibColor(b.color).a == 255) {
            opaqueBorders.push_back({cmd->boundingBox, (float)b.width.left, i});
        }
    }
    auto borderInset = [&](const Clay_RenderCommand *cmd, int index) {
        for (const OpaqueBorder &border : opaqueBorders) {
            const Clay_BoundingBox &a = border.box, &c = cmd->boundingBox;
            if (border.index > index && a.x == c.x && a.y == c.y && a.width == c.width && a.height == c.height) {
                return border.width;
            }
        }
        return 0.0f;
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
        // Snap boxes to whole device pixels. Clay lays out in floats, so sibling
        // edges land on fractional coordinates and rasterize as a half-pixel
        // blur that reads as uneven widths and gaps (three equally-grown
        // cards can each look a different size). Rounding the two edges
        // independently (rather than x and width) keeps adjacent elements
        // flush with each other. Snapping in device rather than logical units
        // matters at fractional scales, where a whole logical coordinate lands
        // mid-pixel and the shape shader's coverage ramp would soften an edge
        // that was meant to be straight.
        float x0 = snapToDevice(cmd->boundingBox.x);
        float y0 = snapToDevice(cmd->boundingBox.y);
        Rectangle rect = {
            x0,
            y0,
            snapToDevice(cmd->boundingBox.x + cmd->boundingBox.width) - x0,
            snapToDevice(cmd->boundingBox.y + cmd->boundingBox.height) - y0,
        };

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Color color = applyOverlay(ToRaylibColor(cmd->renderData.rectangle.backgroundColor));
                const Clay_CornerRadius &cr = cmd->renderData.rectangle.cornerRadius;
                const float inset = borderInset(cmd, i);
                bool sharp = cr.topLeft <= 0.0f && cr.topRight <= 0.0f
                    && cr.bottomRight <= 0.0f && cr.bottomLeft <= 0.0f;
                if (sharp && inset <= 0.0f) {
                    // Snapped edges already sit on device pixels, so the shader
                    // would add a batch flush and change nothing.
                    DrawRectangleRec(rect, color);
                } else if (!DrawRectangleRoundedSDF(rect, cr, inset, color, BLANK, scale)) {
                    DrawRectangleRoundedPerCorner(rect, cr, roundedCornerSegments, color);
                }
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
                if (uniformWidth && b.width.left > 0
                    && DrawRectangleRoundedSDF(rect, b.cornerRadius, (float)b.width.left,
                        BLANK, c, scale)) {
                    break;
                }
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
