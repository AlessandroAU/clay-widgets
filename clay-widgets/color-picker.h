#ifndef CLAY_WIDGETS_COLOR_PICKER_H
#define CLAY_WIDGETS_COLOR_PICKER_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before color-picker.h"
#endif

typedef struct ClayWidgets_ColorPickerOptions {
    bool showAlpha; // false preserves the caller's alpha channel
    bool disabled;
    bool inlinePanel; // default: open a dialog with OK/Cancel
} ClayWidgets_ColorPickerOptions;

// Windows-style colour picker. Channels use Clay's 0..255 range. Returns true on
// edits (on OK in dialog mode); Cancel/Escape discard the draft. Disabled
// pickers never modify the caller's color.
bool ClayWidgets_ColorPicker(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color *color, ClayWidgets_ColorPickerOptions options);

#ifdef CLAY_WIDGETS_IMPLEMENTATION
enum {
    CLAY_WIDGETS__COLOR_PALETTE_COLUMNS = 8,
    CLAY_WIDGETS__COLOR_PALETTE_ROWS = 6,
    CLAY_WIDGETS__COLOR_SPECTRUM_COLUMNS = 64,
    CLAY_WIDGETS__COLOR_SPECTRUM_ROWS = 32,
    CLAY_WIDGETS__COLOR_LUMINANCE_STEPS = 64,
};
static const float ClayWidgets__ColorSpectrumHeight = 160.0f;
static const float ClayWidgets__ColorLuminanceWidth = 24.0f;
static const float ClayWidgets__ColorMarkerSize = 9.0f;
static const float ClayWidgets__ColorKeyStep = 1.0f / 240.0f;

typedef struct ClayWidgets__ColorState {
    float hue, saturation, lightness;
    Clay_Color last, draft;
    bool initialized, open;
    int32_t paletteIndex;
} ClayWidgets__ColorState;

static float ClayWidgets__ColorChannel(float v) {
    return v >= 0 ? ClayWidgets__Clamp(v, 0, 255) : 0;
}

static bool ClayWidgets__ColorEqual(Clay_Color a, Clay_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static Clay_Color ClayWidgets__HslColor(float h, float s, float l, float alpha) {
    float chroma = (1 - fabsf(2 * l - 1)) * s;
    float sector = h * 6;
    float x = chroma * (1 - fabsf(fmodf(sector, 2) - 1));
    float m = l - chroma * 0.5f;
    Clay_Color c = {0, 0, 0, alpha};
    if (sector < 1) { c.r = chroma; c.g = x; }
    else if (sector < 2) { c.r = x; c.g = chroma; }
    else if (sector < 3) { c.g = chroma; c.b = x; }
    else if (sector < 4) { c.g = x; c.b = chroma; }
    else if (sector < 5) { c.r = x; c.b = chroma; }
    else { c.r = chroma; c.b = x; }
    c.r = ClayWidgets__ColorChannel((c.r + m) * 255);
    c.g = ClayWidgets__ColorChannel((c.g + m) * 255);
    c.b = ClayWidgets__ColorChannel((c.b + m) * 255);
    return c;
}

static void ClayWidgets__ColorSync(ClayWidgets__ColorState *state, Clay_Color c) {
    float r = ClayWidgets__ColorChannel(c.r) / 255;
    float g = ClayWidgets__ColorChannel(c.g) / 255;
    float b = ClayWidgets__ColorChannel(c.b) / 255;
    float hi = fmaxf(r, fmaxf(g, b)), lo = fminf(r, fminf(g, b)), d = hi - lo;
    state->lightness = (hi + lo) * 0.5f;
    if (d > 0) {
        state->saturation = d / (1 - fabsf(2 * state->lightness - 1));
        float h = hi == r ? (g - b) / d : hi == g ? (b - r) / d + 2 : (r - g) / d + 4;
        state->hue = (h < 0 ? h + 6 : h) / 6;
    } else if (hi > 0 && lo < 1) {
        state->saturation = 0; // preserve hue through achromatic colours
    }
    state->last = c;
    state->initialized = true;
}

// Shared interaction for the 2D spectrum and the vertical luminance strip.
static bool ClayWidgets__ColorDrag(ClayWidgets_Context *ctx, Clay_ElementId id,
    float *x, float *y, bool vertical, bool disabled) {
    if (disabled) {
        if (ctx->activeId == id.id) ctx->activeId = 0;
        if (ctx->focusedId == id.id) ctx->focusedId = 0;
        return false;
    }
    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    if (over || ctx->activeId == id.id) ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    float beforeX = *x, beforeY = *y;
    if (over && ctx->input.pointerPressed) ClayWidgets__CapturePointer(ctx, id.id);
    if (ctx->activeId == id.id && ctx->input.pointerDown) {
        Clay_ElementData data = Clay_GetElementData(id);
        if (data.found && data.boundingBox.width > 0 && data.boundingBox.height > 0) {
            if (!vertical) *x = ClayWidgets__Clamp((ctx->input.mouseX - data.boundingBox.x) / data.boundingBox.width, 0, 1);
            *y = 1 - ClayWidgets__Clamp((ctx->input.mouseY - data.boundingBox.y) / data.boundingBox.height, 0, 1);
        }
    }
    if (!ctx->input.pointerDown && ctx->activeId == id.id) ctx->activeId = 0;
    if (focused) {
        if (!vertical) *x = ClayWidgets__Clamp(*x + (ctx->input.keyRight ? ClayWidgets__ColorKeyStep : 0) - (ctx->input.keyLeft ? ClayWidgets__ColorKeyStep : 0), 0, 1);
        *y = ClayWidgets__Clamp(*y + (ctx->input.keyUp ? ClayWidgets__ColorKeyStep : 0) - (ctx->input.keyDown ? ClayWidgets__ColorKeyStep : 0), 0, 1);
        if (ctx->input.keyHome) *y = 0;
        if (ctx->input.keyEnd) *y = 1;
        ClayWidgets__FocusRect(ctx, id);
    }
    return beforeX != *x || beforeY != *y;
}

static void ClayWidgets__ColorMarker(ClayWidgets_Context *ctx, Clay_ElementId parent,
    float x, float y, bool vertical) {
    Clay_ElementData data = Clay_GetElementData(parent);
    float width = data.found ? data.boundingBox.width : (vertical ? ClayWidgets__ColorLuminanceWidth : 240.0f);
    float height = data.found ? data.boundingBox.height : ClayWidgets__ColorSpectrumHeight;
    CLAY_AUTO_ID({
        .layout = { .sizing = { .width = CLAY_SIZING_FIXED(vertical ? 30.0f : ClayWidgets__ColorMarkerSize), .height = CLAY_SIZING_FIXED(vertical ? 5.0f : ClayWidgets__ColorMarkerSize) }, .padding = CLAY_PADDING_ALL(1) },
        .backgroundColor = {0, 0, 0, 255},
        .floating = {
            .offset = {vertical ? -3.0f : x * (width - ClayWidgets__ColorMarkerSize), (1 - y) * (height - (vertical ? 5.0f : ClayWidgets__ColorMarkerSize))},
            .parentId = parent.id, .zIndex = ClayWidgets__OverlayZ(ctx, 2),
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
            .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } },
            .border = { .color = {255, 255, 255, 255}, .width = {1, 1, 1, 1, 0} },
        }) {}
    }
}

static void ClayWidgets__ColorPalette(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color *color, bool disabled, ClayWidgets__ColorState *state) {
    ClayWidgets_Label(ctx, CLAY_STRING("Basic colours"));
    static const uint32_t palette[CLAY_WIDGETS__COLOR_PALETTE_ROWS * CLAY_WIDGETS__COLOR_PALETTE_COLUMNS] = {
        0xff8080,0xffff80,0x80ff80,0x00ff80,0x80ffff,0x0080ff,0xff80c0,0xff80ff,
        0xff0000,0xffff00,0x80ff00,0x00ff40,0x00ffff,0x0080c0,0x8080c0,0xff00ff,
        0x804040,0xff8040,0x00ff00,0x008080,0x004080,0x8080ff,0x800040,0xff0080,
        0x800000,0xff8000,0x008000,0x008040,0x0000ff,0x0000a0,0x800080,0x8000ff,
        0x400000,0x804000,0x004000,0x004040,0x000080,0x000040,0x400040,0x400080,
        0x000000,0x808000,0x808040,0x808080,0x408080,0xc0c0c0,0x400040,0xffffff,
    };
    Clay_ElementId paletteId = ClayWidgets__ChildId(id, CLAY_STRING("Palette"), 0);
    bool focused = !disabled && ClayWidgets__RegisterFocusable(ctx, paletteId, false);
    if (disabled && ctx->focusedId == paletteId.id) ctx->focusedId = 0;
    if (focused) {
        int index = state->paletteIndex;
        if (ctx->input.keyLeft && index % CLAY_WIDGETS__COLOR_PALETTE_COLUMNS > 0) --index;
        if (ctx->input.keyRight && index % CLAY_WIDGETS__COLOR_PALETTE_COLUMNS < CLAY_WIDGETS__COLOR_PALETTE_COLUMNS - 1) ++index;
        if (ctx->input.keyUp && index >= CLAY_WIDGETS__COLOR_PALETTE_COLUMNS) index -= CLAY_WIDGETS__COLOR_PALETTE_COLUMNS;
        if (ctx->input.keyDown && index < (CLAY_WIDGETS__COLOR_PALETTE_ROWS - 1) * CLAY_WIDGETS__COLOR_PALETTE_COLUMNS) index += CLAY_WIDGETS__COLOR_PALETTE_COLUMNS;
        if (ctx->input.keyHome) index = 0;
        if (ctx->input.keyEnd) index = CLAY_WIDGETS__COLOR_PALETTE_ROWS * CLAY_WIDGETS__COLOR_PALETTE_COLUMNS - 1;
        state->paletteIndex = index;
    }
    CLAY(paletteId, { .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
        .childGap = ctx->theme.spacing.sm, .layoutDirection = CLAY_TOP_TO_BOTTOM,
    } }) {
        for (int row = 0; row < CLAY_WIDGETS__COLOR_PALETTE_ROWS; ++row) {
            CLAY_AUTO_ID({ .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(20) }, .childGap = 4,
            } }) {
                for (int col = 0; col < CLAY_WIDGETS__COLOR_PALETTE_COLUMNS; ++col) {
                    int index = row * CLAY_WIDGETS__COLOR_PALETTE_COLUMNS + col;
                    uint32_t rgb = palette[index];
                    Clay_Color swatch = {(float)(rgb >> 16), (float)((rgb >> 8) & 255), (float)(rgb & 255), 255};
                    Clay_ElementId swatchId = ClayWidgets__ChildId(id, CLAY_STRING("Swatch"), index);
                    ClayWidgets__ButtonInteraction interaction = ClayWidgets__InteractButton(ctx, swatchId, disabled, paletteId);
                    if (interaction.over && ctx->input.pointerPressed) state->paletteIndex = index;
                    bool highlighted = !disabled && ctx->focusedId == paletteId.id && state->paletteIndex == index;
                    if (interaction.clicked || (highlighted && ClayWidgets__ActivateFocused(ctx, paletteId))) {
                        swatch.a = color->a;
                        *color = swatch;
                        ClayWidgets__ColorSync(state, *color);
                    }
                    swatch.a = 255;
                    bool selected = color->r == swatch.r && color->g == swatch.g && color->b == swatch.b;
                    if (highlighted || selected) ClayWidgets__FocusRect(ctx, swatchId);
                    ClayWidgets_SetEdge(ctx, swatchId, CLAY_WIDGETS_EDGE_SUNKEN_THIN);
                    CLAY(swatchId, {
                        .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } },
                        .backgroundColor = swatch,
                        .border = ClayWidgets__Border(ctx, highlighted || selected ? ctx->theme.focusRingColor : ctx->theme.borderColor),
                    }) {}
                    ClayWidgets__Describe(ctx, swatchId, CLAY_WIDGETS_ROLE_BUTTON, CLAY_STRING("Basic colour"), selected, disabled);
                }
            }
        }
    }

}

static void ClayWidgets__ColorSpectrum(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color *color, bool disabled, ClayWidgets__ColorState *state) {
    ClayWidgets_Label(ctx, CLAY_STRING("Hue / saturation"));
    Clay_ElementId spectrum = ClayWidgets__ChildId(id, CLAY_STRING("Spectrum"), 0);
    Clay_ElementId luminance = ClayWidgets__ChildId(id, CLAY_STRING("Luminance"), 0);
    bool edit = ClayWidgets__ColorDrag(ctx, spectrum, &state->hue, &state->saturation, false, disabled);
    edit |= ClayWidgets__ColorDrag(ctx, luminance, &state->hue, &state->lightness, true, disabled);
    if (edit) *color = ClayWidgets__HslColor(state->hue, state->saturation, state->lightness, color->a);
    CLAY_AUTO_ID({ .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(ClayWidgets__ColorSpectrumHeight) }, .childGap = 12,
    } }) {
            ClayWidgets_SetEdge(ctx, spectrum, CLAY_WIDGETS_EDGE_SUNKEN_THIN);
        CLAY(spectrum, { .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }, .backgroundColor = ctx->theme.fieldColor }) {
            // Ordinary Clay rectangles keep the spectrum renderer-independent.
            for (int y = 0; y < CLAY_WIDGETS__COLOR_SPECTRUM_ROWS; ++y) {
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } } }) {
                    for (int x = 0; x < CLAY_WIDGETS__COLOR_SPECTRUM_COLUMNS; ++x) {
                        Clay_Color cell = ClayWidgets__HslColor((float)x / (CLAY_WIDGETS__COLOR_SPECTRUM_COLUMNS - 1), 1-(float)y / (CLAY_WIDGETS__COLOR_SPECTRUM_ROWS - 1), 0.5f, 255);
                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } }, .backgroundColor = cell }) {}
                    }
                }
            }
            ClayWidgets__ColorMarker(ctx, spectrum, state->hue, state->saturation, false);
        }
        ClayWidgets_SetEdge(ctx, luminance, CLAY_WIDGETS_EDGE_SUNKEN_THIN);
        CLAY(luminance, { .layout = {
            .sizing = { .width = CLAY_SIZING_FIXED(ClayWidgets__ColorLuminanceWidth), .height = CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }, .backgroundColor = ctx->theme.fieldColor }) {
            for (int y = 0; y < CLAY_WIDGETS__COLOR_LUMINANCE_STEPS; ++y) {
                Clay_Color cell = ClayWidgets__HslColor(state->hue, state->saturation, 1-(float)y / (CLAY_WIDGETS__COLOR_LUMINANCE_STEPS - 1), 255);
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } }, .backgroundColor = cell }) {}
            }
            ClayWidgets__ColorMarker(ctx, luminance, 0, state->lightness, true);
        }
    }
    ClayWidgets__Describe(ctx, spectrum, CLAY_WIDGETS_ROLE_SLIDER, CLAY_STRING("Hue and saturation"), false, disabled);
    ClayWidgets__Describe(ctx, luminance, CLAY_WIDGETS_ROLE_SLIDER, CLAY_STRING("Luminance"), false, disabled);
}

typedef struct ClayWidgets__HexState {
    char text[10];
    uint32_t last, frame;
    bool initialized, focused, alpha, refresh;
} ClayWidgets__HexState;

static bool ClayWidgets__HexText(const char *text, int32_t length, void *data) {
    int32_t maxDigits = *(bool *)data ? 8 : 6;
    int32_t first = length && text[0] == '#' ? 1 : 0;
    if (length - first > maxDigits) return false;
    for (int32_t i = first; i < length; ++i) {
        char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

static void ClayWidgets__ColorHex(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color *color, bool alpha, bool disabled, ClayWidgets__ColorState *state) {
    ClayWidgets__HexState *hex = (ClayWidgets__HexState *)ClayWidgets_GetState(ctx, id.id, sizeof(ClayWidgets__HexState));
    if (!hex) return;
    if (hex->initialized && ctx->animFrame - hex->frame > 1) {
        hex->initialized = false;
        hex->focused = false;
    }
    hex->frame = ctx->animFrame;
    uint32_t packed = ((uint32_t)roundf(ClayWidgets__ColorChannel(color->r)) << 16)
        | ((uint32_t)roundf(ClayWidgets__ColorChannel(color->g)) << 8)
        | (uint32_t)roundf(ClayWidgets__ColorChannel(color->b));
    if (alpha) packed = (packed << 8) | (uint32_t)roundf(ClayWidgets__ColorChannel(color->a));
    if (!hex->initialized || hex->last != packed || hex->alpha != alpha || disabled || hex->refresh) {
        snprintf(hex->text, sizeof(hex->text), alpha ? "#%08X" : "#%06X", (unsigned)packed);
        hex->initialized = true;
        hex->refresh = false;
    }
    ClayWidgets_EditResult result = {0};
    ClayWidgets_TextInputOptions options = {0};
    options.disabled = disabled; options.validate = ClayWidgets__HexText;
    options.validationUserData = &alpha; options.result = &result;
    ClayWidgets_TextInput(ctx, id, alpha ? CLAY_STRING("Hex (RRGGBBAA)") : CLAY_STRING("Hex (RRGGBB)"), hex->text, sizeof(hex->text), options);
    bool focused = !disabled && ctx->focusedId == id.id;
    if (!disabled && result.changed) {
        const char *digits = hex->text + (hex->text[0] == '#');
        if (strlen(digits) == (alpha ? 8u : 6u)) {
            packed = (uint32_t)strtoul(digits, NULL, 16);
            uint32_t rgb = alpha ? packed >> 8 : packed;
            *color = (Clay_Color){(float)(rgb >> 16), (float)((rgb >> 8) & 255), (float)(rgb & 255), alpha ? (float)(packed & 255) : color->a};
            ClayWidgets__ColorSync(state, *color);
        }
    }
    if (result.submitted || result.cancelled || (hex->focused && !focused)) hex->refresh = true;
    hex->focused = focused; hex->last = packed; hex->alpha = alpha;
}

static void ClayWidgets__ColorControls(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color *color, ClayWidgets_ColorPickerOptions options, bool narrow, bool disabled, ClayWidgets__ColorState *state) {
    float channels[4] = {color->r, color->g, color->b, color->a};
    Clay_String labels[3] = {CLAY_STRING("Red"), CLAY_STRING("Green"), CLAY_STRING("Blue")};
    CLAY_AUTO_ID({ .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
        .childGap = ctx->theme.spacing.md, .layoutDirection = narrow ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
    } }) {
        CLAY_AUTO_ID({ .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
            .childGap = ctx->theme.spacing.sm,
        } }) {
            for (int i = 0; i < 3; ++i) {
                int32_t value = (int32_t)roundf(ClayWidgets__ColorChannel(channels[i]));
                if (ClayWidgets_NumberInput(ctx, ClayWidgets__ChildId(id, CLAY_STRING("Channel"), i), labels[i], &value,
                    (ClayWidgets_NumberInputOptions){0, 255, 1, disabled})) {
                    channels[i] = (float)value;
                    *color = (Clay_Color){channels[0], channels[1], channels[2], channels[3]};
                    ClayWidgets__ColorSync(state, *color);
                }
            }
        }
        CLAY_AUTO_ID({
            .layout = { .sizing = {
                .width = narrow ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(1),
                .height = narrow ? CLAY_SIZING_FIXED(1) : CLAY_SIZING_GROW(0),
            } },
            .backgroundColor = ctx->theme.borderColor,
        }) {}
        CLAY_AUTO_ID({ .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
            .childGap = ctx->theme.spacing.sm,
        } }) {
            Clay_String hslLabels[3] = {CLAY_STRING("Hue"), CLAY_STRING("Sat %"), CLAY_STRING("Lum %")};
            float *hsl[3] = {&state->hue, &state->saturation, &state->lightness};
            for (int i = 0; i < 3; ++i) {
                int32_t limit = i == 0 ? 360 : 100;
                int32_t value = (int32_t)roundf(*hsl[i] * limit);
                if (ClayWidgets_NumberInput(ctx, ClayWidgets__ChildId(id, CLAY_STRING("HSL"), i), hslLabels[i], &value,
                    (ClayWidgets_NumberInputOptions){0, limit, 1, disabled})) {
                    *hsl[i] = (float)value / limit;
                    *color = ClayWidgets__HslColor(state->hue, state->saturation, state->lightness, color->a);
                }
            }
        }
    }
    ClayWidgets__ColorHex(ctx, ClayWidgets__ChildId(id, CLAY_STRING("Hex"), 0), color, options.showAlpha, disabled, state);
    if (options.showAlpha) {
        ClayWidgets_Label(ctx, CLAY_STRING("Alpha"));
        float alphaValue = ClayWidgets__ColorChannel(color->a);
        if (ClayWidgets_Slider(ctx, ClayWidgets__ChildId(id, CLAY_STRING("Channel"), 3), &alphaValue,
            (ClayWidgets_SliderOptions){0,255,1,true,0,disabled}) && !disabled) color->a = alphaValue;
    }

}

static void ClayWidgets__ColorPreview(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color color, Clay_String label) {
    ClayWidgets_Label(ctx, label);
    // Composite over checkerboard cells using ordinary Clay rectangles,
    // keeping the preview portable across renderers and scroll clipping.
    Clay_ElementId previewId = ClayWidgets__ChildId(id, CLAY_STRING("Preview"), 0);
    float alpha = color.a >= 0 ? ClayWidgets__Clamp(color.a / 255.0f, 0, 1) : 0;
    Clay_Color fills[2];
    for (int i = 0; i < 2; ++i) {
        float base = i ? 180.0f : 230.0f;
        fills[i] = (Clay_Color){
            base + (ClayWidgets__ColorChannel(color.r) - base) * alpha,
            base + (ClayWidgets__ColorChannel(color.g) - base) * alpha,
            base + (ClayWidgets__ColorChannel(color.b) - base) * alpha, 255};
    }
    CLAY(previewId, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(32) },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        // A continuous fill prevents fractional cell boundaries exposing the
        // surface underneath. Opaque colours need only this single rectangle.
        .backgroundColor = fills[0],
    }) {
        for (int32_t row = 0; alpha < 1 && row < 2; ++row) {
            CLAY_AUTO_ID({ .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            } }) {
                for (int32_t col = 0; col < 12; ++col) {
                    CLAY_AUTO_ID({
                        .layout = { .sizing = {
                            .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0),
                        } },
                        .backgroundColor = fills[(row + col) % 2],
                    }) {}
                }
            }
        }
    }
}

static bool ClayWidgets__ColorPanel(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color *color, ClayWidgets_ColorPickerOptions options, ClayWidgets__ColorState *state, const Clay_Color *original) {
    bool disabled = options.disabled || ctx->disabledDepth > 0;
    Clay_Color before = *color;
    if (!disabled) {
        color->r = ClayWidgets__ColorChannel(color->r);
        color->g = ClayWidgets__ColorChannel(color->g);
        color->b = ClayWidgets__ColorChannel(color->b);
        if (options.showAlpha) color->a = ClayWidgets__ColorChannel(color->a);
    }
    if (!state->initialized || !ClayWidgets__ColorEqual(*color, state->last)) ClayWidgets__ColorSync(state, *color);
    Clay_ElementData previous = Clay_GetElementData(id);
    bool narrow = previous.found ? previous.boundingBox.width < 480 : ctx->layoutDimensions.width < 560;
    CLAY(id, { .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
        .childGap = ctx->theme.spacing.sm, .layoutDirection = CLAY_TOP_TO_BOTTOM,
    } }) {
        CLAY_AUTO_ID({ .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
            .childGap = ctx->theme.spacing.md, .layoutDirection = narrow ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
        } }) {
            CLAY_AUTO_ID({ .layout = {
                .sizing = { .width = narrow ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(200), .height = CLAY_SIZING_FIT(0) },
                .childGap = ctx->theme.spacing.sm, .layoutDirection = CLAY_TOP_TO_BOTTOM,
            } }) {
                ClayWidgets__ColorPalette(ctx, id, color, disabled, state);
            }
            CLAY_AUTO_ID({ .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                .childGap = ctx->theme.spacing.sm, .layoutDirection = CLAY_TOP_TO_BOTTOM,
            } }) {
                ClayWidgets__ColorSpectrum(ctx, id, color, disabled, state);
            }
        }
        ClayWidgets__ColorControls(ctx, id, color, options, narrow, disabled, state);
        CLAY_AUTO_ID({ .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) }, .childGap = ctx->theme.spacing.sm,
        } }) {
            if (original) {
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                    ClayWidgets__ColorPreview(ctx, ClayWidgets__ChildId(id, CLAY_STRING("Original"), 0), *original, CLAY_STRING("Original"));
                }
            }
            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                ClayWidgets__ColorPreview(ctx, id, *color, CLAY_STRING("New colour"));
            }
        }
    }
    state->last = *color;
    return !disabled && !ClayWidgets__ColorEqual(before, *color);
}

bool ClayWidgets_ColorPicker(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_Color *color, ClayWidgets_ColorPickerOptions options) {
    if (!ctx || !color) return false;
    ClayWidgets__ColorState *state = (ClayWidgets__ColorState *)ClayWidgets_GetState(ctx, id.id, sizeof(ClayWidgets__ColorState));
    if (!state) return false;
    if (options.inlinePanel) return ClayWidgets__ColorPanel(ctx, id, color, options, state, NULL);
    bool disabled = options.disabled || ctx->disabledDepth > 0;
    if (disabled) state->open = false;
    Clay_ElementId trigger = ClayWidgets__ChildId(id, CLAY_STRING("Trigger"), 0);
    ClayWidgets__ButtonInteraction interaction = ClayWidgets__InteractButton(ctx, trigger, disabled, trigger);
    bool over = interaction.over;
    bool focused = interaction.focused;
    Clay_Color *original = (Clay_Color *)ClayWidgets_GetState(ctx,
        ClayWidgets__ChildId(id, CLAY_STRING("Original"), 0).id, sizeof(Clay_Color));
    if (interaction.clicked) {
        if (original) *original = *color;
        state->draft = *color;
        state->open = true;
        state->initialized = false;
    }
    ClayWidgets_SetEdge(ctx, trigger, CLAY_WIDGETS_EDGE_RAISED);
    if (focused) ClayWidgets__FocusRect(ctx, trigger);
    CLAY(trigger, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm), .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = over ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
        .border = ClayWidgets__Border(ctx, focused ? ctx->theme.focusRingColor : ctx->theme.borderColor),
    }) {
        CLAY_AUTO_ID({
            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(32), .height = CLAY_SIZING_FIXED(22) } },
            .backgroundColor = *color,
            .border = ClayWidgets__Border(ctx, ctx->theme.borderColor),
        }) {}
        CLAY_TEXT(CLAY_STRING("Choose colour..."), {
            .textColor = disabled ? ctx->theme.textMutedColor : ctx->theme.textColor,
            .fontId = ctx->theme.fontBody, .fontSize = ctx->theme.fontSizeBody,
        });
    }
    ClayWidgets__Describe(ctx, trigger, CLAY_WIDGETS_ROLE_BUTTON, CLAY_STRING("Choose colour"), false, disabled);
    bool changed = false;
    Clay_ElementId dialog = ClayWidgets__ChildId(id, CLAY_STRING("Dialog"), 0);
    if (ClayWidgets_BeginModalEx(ctx, dialog, CLAY_STRING("Colour"), &state->open, (ClayWidgets_ModalOptions){true, 640})) {
        Clay_ElementId scroll = ClayWidgets__ChildId(id, CLAY_STRING("Scroll"), 0);
        ClayWidgets_BeginScrollPanel(ctx, scroll, (ClayWidgets_ScrollPanelOptions){
            CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0, fmaxf(40, ctx->layoutDimensions.height - 180)),
            1, ctx->theme.spacing.sm, ctx->theme.spacing.sm});
        ClayWidgets__ColorPanel(ctx, ClayWidgets__ChildId(id, CLAY_STRING("Panel"), 0), &state->draft, options, state, original);
        ClayWidgets_EndScrollPanel(ctx, scroll);
        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) }, .childGap = ctx->theme.spacing.sm, .childAlignment = { .x = CLAY_ALIGN_X_RIGHT } } }) {
            if (ClayWidgets_Button(ctx, ClayWidgets__ChildId(id, CLAY_STRING("Cancel"), 0), CLAY_STRING("Cancel"))) state->open = false;
            if (ClayWidgets_ButtonEx(ctx, ClayWidgets__ChildId(id, CLAY_STRING("OK"), 0), CLAY_STRING("OK"), (ClayWidgets_ButtonOptions){CLAY_WIDGETS_BUTTON_PRIMARY, false})) {
                changed = !ClayWidgets__ColorEqual(*color, state->draft); *color = state->draft; state->open = false;
            }
        }
        ClayWidgets_EndModal(ctx, dialog);
    }
    return changed;
}
#endif
#endif
