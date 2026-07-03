#ifndef CLAY_WIDGETS_IMAGE_H
#define CLAY_WIDGETS_IMAGE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before image.h"
#endif

// An image / icon element. `imageData` is an opaque pointer handed straight
// through to your renderer's IMAGE render command (for the raylib demo it is a
// Texture2D*). The widget layer never dereferences it, so any backend can pass
// its own texture handle.
//
// ClayWidgets_Image draws the texture at width x height with no tint.
// ClayWidgets_Icon draws it square at `size` and tints it by `tint`, so a white
// glyph texture can be recolored to match the theme (e.g. pass
// ctx->theme.accentColor).
//
// Tint transport: this Clay build emits an extra RECTANGLE whenever an element's
// backgroundColor alpha is > 0, which would paint over the image - so the tint
// is NOT sent via backgroundColor. Instead it is packed as 0xRRGGBBAA into the
// element's userData, which Clay forwards to the IMAGE render command. A value
// of 0 means "untinted". Renderers opt in by decoding cmd->userData; those that
// ignore it simply draw the texture untinted. Use ClayWidgets_PackTint /
// ClayWidgets_UnpackTint so the demo and the widgets agree on the layout.
static inline uintptr_t ClayWidgets__TintByte(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (uintptr_t)v;
}

static inline void *ClayWidgets_PackTint(Clay_Color tint) {
    uintptr_t r = ClayWidgets__TintByte(tint.r);
    uintptr_t g = ClayWidgets__TintByte(tint.g);
    uintptr_t b = ClayWidgets__TintByte(tint.b);
    uintptr_t a = ClayWidgets__TintByte(tint.a);
    return (void *)((r << 24) | (g << 16) | (b << 8) | a);
}

static inline Clay_Color ClayWidgets_UnpackTint(void *packed) {
    uintptr_t v = (uintptr_t)packed;
    Clay_Color c = {
        (float)((v >> 24) & 0xFFu),
        (float)((v >> 16) & 0xFFu),
        (float)((v >> 8) & 0xFFu),
        (float)(v & 0xFFu),
    };
    return c;
}

void ClayWidgets_ImageEx(ClayWidgets_Context *ctx, Clay_ElementId id, void *imageData, float width, float height, Clay_Color tint);
void ClayWidgets_Image(ClayWidgets_Context *ctx, Clay_ElementId id, void *imageData, float width, float height);
void ClayWidgets_Icon(ClayWidgets_Context *ctx, Clay_ElementId id, void *imageData, float size, Clay_Color tint);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

void ClayWidgets_ImageEx(ClayWidgets_Context *ctx, Clay_ElementId id, void *imageData, float width, float height, Clay_Color tint) {
    if (!ctx || !imageData) {
        return;
    }
    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height) },
        },
        .image = { .imageData = imageData },
        .userData = ClayWidgets_PackTint(tint),
    }) {}
}

void ClayWidgets_Image(ClayWidgets_Context *ctx, Clay_ElementId id, void *imageData, float width, float height) {
    // Untinted: a zero tint packs to a null userData, which renderers read as
    // "draw the texture as-is".
    ClayWidgets_ImageEx(ctx, id, imageData, width, height, (Clay_Color){0, 0, 0, 0});
}

void ClayWidgets_Icon(ClayWidgets_Context *ctx, Clay_ElementId id, void *imageData, float size, Clay_Color tint) {
    ClayWidgets_ImageEx(ctx, id, imageData, size, size, tint);
}

#endif

#endif
