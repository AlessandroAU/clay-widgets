#ifndef CLAY_WIDGETS_H
#define CLAY_WIDGETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clay.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLAY_WIDGETS_TEXT_MAX_BYTES
#define CLAY_WIDGETS_TEXT_MAX_BYTES 2048
#endif

#ifndef CLAY_WIDGETS_MAX_FOCUSABLES
#define CLAY_WIDGETS_MAX_FOCUSABLES 128
#endif

typedef struct ClayWidgets_Input {
    float mouseX;
    float mouseY;
    bool pointerDown;
    bool pointerPressed;
    bool pointerReleased;
    float scrollX;
    float scrollY;
    float deltaTime;

    const char *textUtf8;
    int32_t textUtf8Length;

    bool keyBackspace;
    bool keyDelete;
    bool keyHome;
    bool keyEnd;
    bool keyLeft;
    bool keyRight;
    bool keyEnter;
    bool keyEscape;
    bool keyTab;
    bool keySelectAll;
    bool shiftDown;
} ClayWidgets_Input;

typedef Clay_Dimensions (*ClayWidgets_MeasureTextFunction)(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

typedef struct ClayWidgets_Spacing {
    uint16_t xs;
    uint16_t sm;
    uint16_t md;
    uint16_t lg;
} ClayWidgets_Spacing;

typedef struct ClayWidgets_Theme {
    Clay_Color textColor;
    Clay_Color textMutedColor;
    Clay_Color surfaceColor;
    Clay_Color surfaceAltColor;
    Clay_Color accentColor;
    Clay_Color accentMutedColor;
    Clay_Color borderColor;
    Clay_Color hoverColor;
    Clay_Color pressedColor;
    Clay_Color focusRingColor;

    uint16_t radiusSm;
    uint16_t radiusMd;

    uint16_t fontBody;
    uint16_t fontHeading;
    uint16_t fontMono;

    uint16_t fontSizeBody;
    uint16_t fontSizeHeading;
    uint16_t fontSizeSmall;

    ClayWidgets_Spacing spacing;
} ClayWidgets_Theme;

typedef struct ClayWidgets_Context {
    ClayWidgets_Input input;
    ClayWidgets_Theme theme;
    ClayWidgets_MeasureTextFunction measureText;
    void *measureTextUserData;

    uint32_t activeId;
    uint32_t focusedId;
    uint32_t focusOrder[CLAY_WIDGETS_MAX_FOCUSABLES];
    int32_t focusCount;
    uint32_t textInputId;
    int32_t textCursor;
    int32_t textSelectionAnchor;
    float textScrollX;
    bool textPointerSelecting;
    float caretBlinkTime;
    float elapsedTime;
    uint32_t lastClickId;
    float lastClickTime;
    float lastClickX;
    float lastClickY;
    bool clickConsumed;
} ClayWidgets_Context;

typedef struct ClayWidgets_SliderOptions {
    float minValue;
    float maxValue;
    float step;
} ClayWidgets_SliderOptions;

typedef struct ClayWidgets_TextInputOptions {
    const char *placeholder;
    bool clearOnEnter;
} ClayWidgets_TextInputOptions;

#include "clay-widgets-themes.h"

void ClayWidgets_Init(ClayWidgets_Context *ctx, ClayWidgets_Theme theme);
void ClayWidgets_SetMeasureTextFunction(ClayWidgets_Context *ctx, ClayWidgets_MeasureTextFunction measureText, void *userData);

void ClayWidgets_BeginFrame(
    ClayWidgets_Context *ctx,
    ClayWidgets_Input input,
    Clay_Dimensions layoutSize,
    bool enableDragScroll
);

Clay_RenderCommandArray ClayWidgets_EndFrame(float deltaTime);

void ClayWidgets_Label(ClayWidgets_Context *ctx, Clay_String text);
void ClayWidgets_Heading(ClayWidgets_Context *ctx, Clay_String text);
void ClayWidgets_Separator(ClayWidgets_Context *ctx);

bool ClayWidgets_Button(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text);
bool ClayWidgets_Checkbox(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value);
bool ClayWidgets_Radio(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue
);

float ClayWidgets_Slider(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    float value,
    ClayWidgets_SliderOptions options
);

void ClayWidgets_ProgressBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    float progress01,
    Clay_String label
);

bool ClayWidgets_TextInput(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    char *buffer,
    int32_t capacity,
    ClayWidgets_TextInputOptions options
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

#include <math.h>
#include <string.h>

static float ClayWidgets__Clamp(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static int32_t ClayWidgets__StrLenBounded(const char *text, int32_t maxLength) {
    int32_t len = 0;
    if (!text || maxLength <= 0) {
        return 0;
    }
    while (len < maxLength && text[len] != '\0') {
        len++;
    }
    return len;
}

static Clay_String ClayWidgets__StringFromCString(const char *text) {
    Clay_String result = {0};
    if (!text) {
        result.chars = "";
        result.length = 0;
        result.isStaticallyAllocated = true;
        return result;
    }
    result.chars = text;
    result.length = ClayWidgets__StrLenBounded(text, CLAY_WIDGETS_TEXT_MAX_BYTES);
    result.isStaticallyAllocated = false;
    return result;
}

static Clay_Color ClayWidgets__MixColor(Clay_Color a, Clay_Color b, float t) {
    float clamped = ClayWidgets__Clamp(t, 0.0f, 1.0f);
    Clay_Color mixed = {
        .r = a.r + (b.r - a.r) * clamped,
        .g = a.g + (b.g - a.g) * clamped,
        .b = a.b + (b.b - a.b) * clamped,
        .a = a.a + (b.a - a.a) * clamped,
    };
    return mixed;
}

static bool ClayWidgets__ConsumeClick(ClayWidgets_Context *ctx, bool over) {
    if (!ctx) {
        return false;
    }
    if (ctx->input.pointerReleased && over && !ctx->clickConsumed) {
        ctx->clickConsumed = true;
        return true;
    }
    return false;
}

static int32_t ClayWidgets__MinI32(int32_t a, int32_t b) {
    return a < b ? a : b;
}

static int32_t ClayWidgets__MaxI32(int32_t a, int32_t b) {
    return a > b ? a : b;
}

static bool ClayWidgets__IsUtf8ContinuationByte(unsigned char byte) {
    return (byte & 0xC0u) == 0x80u;
}

static bool ClayWidgets__IsWordByte(unsigned char byte) {
    return (byte >= '0' && byte <= '9')
        || (byte >= 'A' && byte <= 'Z')
        || (byte >= 'a' && byte <= 'z')
        || byte == '_'
        || byte >= 0x80u;
}

static int32_t ClayWidgets__Utf8PrevBoundary(const char *text, int32_t offset) {
    if (!text || offset <= 0) {
        return 0;
    }
    int32_t result = offset - 1;
    while (result > 0 && ClayWidgets__IsUtf8ContinuationByte((unsigned char)text[result])) {
        result--;
    }
    return result;
}

static int32_t ClayWidgets__Utf8NextBoundary(const char *text, int32_t length, int32_t offset) {
    if (!text || offset >= length) {
        return length;
    }
    int32_t result = offset + 1;
    while (result < length && ClayWidgets__IsUtf8ContinuationByte((unsigned char)text[result])) {
        result++;
    }
    return result;
}

static void ClayWidgets__FindWordBounds(const char *text, int32_t length, int32_t cursor, int32_t *start, int32_t *end) {
    if (!start || !end) {
        return;
    }

    *start = ClayWidgets__MaxI32(0, ClayWidgets__MinI32(cursor, length));
    *end = *start;
    if (!text || length <= 0) {
        return;
    }

    int32_t probe = *start;
    if (probe == length && probe > 0) {
        probe = ClayWidgets__Utf8PrevBoundary(text, probe);
    }

    if (probe < length && !ClayWidgets__IsWordByte((unsigned char)text[probe]) && probe > 0) {
        int32_t previous = ClayWidgets__Utf8PrevBoundary(text, probe);
        if (ClayWidgets__IsWordByte((unsigned char)text[previous])) {
            probe = previous;
        }
    }

    if (probe >= length || !ClayWidgets__IsWordByte((unsigned char)text[probe])) {
        return;
    }

    int32_t wordStart = probe;
    while (wordStart > 0) {
        int32_t previous = ClayWidgets__Utf8PrevBoundary(text, wordStart);
        if (!ClayWidgets__IsWordByte((unsigned char)text[previous])) {
            break;
        }
        wordStart = previous;
    }

    int32_t wordEnd = ClayWidgets__Utf8NextBoundary(text, length, probe);
    while (wordEnd < length && ClayWidgets__IsWordByte((unsigned char)text[wordEnd])) {
        wordEnd = ClayWidgets__Utf8NextBoundary(text, length, wordEnd);
    }

    *start = wordStart;
    *end = wordEnd;
}

static void ClayWidgets__ClampSelectionToLength(ClayWidgets_Context *ctx, int32_t length) {
    if (!ctx) {
        return;
    }
    ctx->textCursor = ClayWidgets__MaxI32(0, ClayWidgets__MinI32(ctx->textCursor, length));
    ctx->textSelectionAnchor = ClayWidgets__MaxI32(0, ClayWidgets__MinI32(ctx->textSelectionAnchor, length));
}

static void ClayWidgets__SetCaret(ClayWidgets_Context *ctx, uint32_t id, int32_t offset, bool keepSelection) {
    if (!ctx) {
        return;
    }
    if (ctx->textInputId != id) {
        ctx->textInputId = id;
        ctx->textSelectionAnchor = offset;
        ctx->textScrollX = 0.0f;
    }
    ctx->textCursor = offset;
    if (!keepSelection) {
        ctx->textSelectionAnchor = offset;
    }
    ctx->caretBlinkTime = 0.0f;
}

static float ClayWidgets__MeasureWidth(
    ClayWidgets_Context *ctx,
    const char *text,
    int32_t length,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
);

static float ClayWidgets__ClampF32(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static void ClayWidgets__UpdateTextScroll(
    ClayWidgets_Context *ctx,
    uint32_t id,
    const char *text,
    int32_t length,
    float availableWidth,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing,
    bool pointerSelecting,
    float pointerLocalX
) {
    if (!ctx || ctx->textInputId != id) {
        return;
    }

    float totalWidth = ClayWidgets__MeasureWidth(ctx, text, length, fontId, fontSize, letterSpacing);
    if (totalWidth <= availableWidth || availableWidth <= 0.0f) {
        ctx->textScrollX = 0.0f;
        return;
    }

    float maxScroll = totalWidth - availableWidth;
    float scrollX = ClayWidgets__ClampF32(ctx->textScrollX, 0.0f, maxScroll);

    if (pointerSelecting) {
        /* Drive scroll from the pointer, NOT the caret. A pointer that is held
         * still inside the field produces no scroll change, so the caret cannot
         * "walk" through a feedback loop (scroll -> caret -> scroll). Only when
         * the pointer is dragged past a content edge do we auto-scroll to reveal
         * more text. */
        if (pointerLocalX < 0.0f) {
            scrollX += pointerLocalX;
        } else if (pointerLocalX > availableWidth) {
            scrollX += (pointerLocalX - availableWidth);
        }
    } else {
        /* Keyboard / programmatic caret movement: keep the caret in view. */
        float caretX = ClayWidgets__MeasureWidth(ctx, text, ctx->textCursor, fontId, fontSize, letterSpacing);
        if (caretX < scrollX) {
            scrollX = caretX;
        } else if (caretX > scrollX + availableWidth) {
            scrollX = caretX - availableWidth;
        }
    }

    ctx->textScrollX = ClayWidgets__ClampF32(scrollX, 0.0f, maxScroll);
}

static bool ClayWidgets__HasSelection(const ClayWidgets_Context *ctx) {
    return ctx && ctx->textCursor != ctx->textSelectionAnchor;
}

static void ClayWidgets__SelectionRange(const ClayWidgets_Context *ctx, int32_t *start, int32_t *end) {
    if (!ctx || !start || !end) {
        return;
    }
    *start = ClayWidgets__MinI32(ctx->textCursor, ctx->textSelectionAnchor);
    *end = ClayWidgets__MaxI32(ctx->textCursor, ctx->textSelectionAnchor);
}

static Clay_Dimensions ClayWidgets__MeasureSlice(
    ClayWidgets_Context *ctx,
    const char *text,
    int32_t length,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
) {
    Clay_Dimensions out = {0};
    if (!ctx || !ctx->measureText || !text || length <= 0) {
        return out;
    }
    Clay_TextElementConfig config = {0};
    config.fontId = fontId;
    config.fontSize = fontSize;
    config.letterSpacing = letterSpacing;
    config.wrapMode = CLAY_TEXT_WRAP_NONE;
    return ctx->measureText((Clay_StringSlice){ .length = length, .chars = text, .baseChars = text }, &config, ctx->measureTextUserData);
}

static float ClayWidgets__MeasureWidth(
    ClayWidgets_Context *ctx,
    const char *text,
    int32_t length,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
) {
    return ClayWidgets__MeasureSlice(ctx, text, length, fontId, fontSize, letterSpacing).width;
}

static int32_t ClayWidgets__FindCursorFromLocalX(
    ClayWidgets_Context *ctx,
    const char *text,
    int32_t length,
    float localX,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
) {
    if (!text || length <= 0 || localX <= 0.0f) {
        return 0;
    }

    float previousWidth = 0.0f;
    int32_t offset = 0;
    while (offset < length) {
        int32_t next = ClayWidgets__Utf8NextBoundary(text, length, offset);
        float nextWidth = ClayWidgets__MeasureWidth(ctx, text, next, fontId, fontSize, letterSpacing);
        float midpoint = previousWidth + (nextWidth - previousWidth) * 0.5f;
        if (localX < midpoint) {
            return offset;
        }
        previousWidth = nextWidth;
        offset = next;
    }

    return length;
}

static bool ClayWidgets__DeleteSelection(char *buffer, int32_t *length, ClayWidgets_Context *ctx) {
    if (!buffer || !length || !ctx || !ClayWidgets__HasSelection(ctx)) {
        return false;
    }
    int32_t start = 0;
    int32_t end = 0;
    ClayWidgets__SelectionRange(ctx, &start, &end);
    memmove(buffer + start, buffer + end, (size_t)(*length - end + 1));
    *length -= (end - start);
    ClayWidgets__SetCaret(ctx, ctx->textInputId, start, false);
    return true;
}

static bool ClayWidgets__InsertTextAtCaret(
    ClayWidgets_Context *ctx,
    uint32_t id,
    char *buffer,
    int32_t *length,
    int32_t capacity,
    const char *text,
    int32_t textLength
) {
    if (!ctx || !buffer || !length || capacity <= 0 || !text || textLength <= 0) {
        return false;
    }

    ClayWidgets__ClampSelectionToLength(ctx, *length);
    ClayWidgets__DeleteSelection(buffer, length, ctx);

    int32_t insertAt = ctx->textCursor;
    int32_t available = capacity - 1 - *length;
    int32_t toCopy = textLength < available ? textLength : available;
    if (toCopy <= 0) {
        return false;
    }

    memmove(buffer + insertAt + toCopy, buffer + insertAt, (size_t)(*length - insertAt + 1));
    memcpy(buffer + insertAt, text, (size_t)toCopy);
    *length += toCopy;
    ClayWidgets__SetCaret(ctx, id, insertAt + toCopy, false);
    return true;
}

static bool ClayWidgets__WasDoubleClick(ClayWidgets_Context *ctx, uint32_t id) {
    if (!ctx) {
        return false;
    }
    float dx = ctx->input.mouseX - ctx->lastClickX;
    float dy = ctx->input.mouseY - ctx->lastClickY;
    float distanceSquared = dx * dx + dy * dy;
    return ctx->lastClickId == id
        && (ctx->elapsedTime - ctx->lastClickTime) <= 0.30f
        && distanceSquared <= 36.0f;
}

static void ClayWidgets__RecordClick(ClayWidgets_Context *ctx, uint32_t id) {
    if (!ctx) {
        return;
    }
    ctx->lastClickId = id;
    ctx->lastClickTime = ctx->elapsedTime;
    ctx->lastClickX = ctx->input.mouseX;
    ctx->lastClickY = ctx->input.mouseY;
}

static int32_t ClayWidgets__FindFocusableIndex(const ClayWidgets_Context *ctx, uint32_t id) {
    if (!ctx) {
        return -1;
    }
    for (int32_t index = 0; index < ctx->focusCount; ++index) {
        if (ctx->focusOrder[index] == id) {
            return index;
        }
    }
    return -1;
}

static void ClayWidgets__AdvanceFocus(ClayWidgets_Context *ctx) {
    if (!ctx || !ctx->input.keyTab || ctx->focusCount <= 0) {
        return;
    }

    int32_t currentIndex = ClayWidgets__FindFocusableIndex(ctx, ctx->focusedId);
    int32_t nextIndex = (currentIndex >= 0) ? (currentIndex + 1) % ctx->focusCount : 0;
    ctx->focusedId = ctx->focusOrder[nextIndex];
}

static bool ClayWidgets__RegisterFocusable(ClayWidgets_Context *ctx, Clay_ElementId id, bool over) {
    if (!ctx) {
        return false;
    }

    if (ctx->focusCount < CLAY_WIDGETS_MAX_FOCUSABLES) {
        ctx->focusOrder[ctx->focusCount++] = id.id;
    }

    if (ctx->input.pointerPressed && over) {
        ctx->focusedId = id.id;
    }

    return ctx->focusedId == id.id;
}

static bool ClayWidgets__ActivateFocused(ClayWidgets_Context *ctx, Clay_ElementId id) {
    return ctx && ctx->focusedId == id.id && ctx->input.keyEnter;
}

void ClayWidgets_Init(ClayWidgets_Context *ctx, ClayWidgets_Theme theme) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->theme = theme;
}

void ClayWidgets_SetMeasureTextFunction(ClayWidgets_Context *ctx, ClayWidgets_MeasureTextFunction measureText, void *userData) {
    if (!ctx) {
        return;
    }
    ctx->measureText = measureText;
    ctx->measureTextUserData = userData;
}

void ClayWidgets_BeginFrame(
    ClayWidgets_Context *ctx,
    ClayWidgets_Input input,
    Clay_Dimensions layoutSize,
    bool enableDragScroll
) {
    if (!ctx) {
        return;
    }

    ctx->input = input;
    ctx->elapsedTime += input.deltaTime;
    ClayWidgets__AdvanceFocus(ctx);
    ctx->focusCount = 0;
    if (ctx->focusedId == ctx->textInputId) {
        ctx->caretBlinkTime += input.deltaTime;
        if (ctx->caretBlinkTime > 1.0f) {
            ctx->caretBlinkTime = fmodf(ctx->caretBlinkTime, 1.0f);
        }
    } else {
        ctx->textPointerSelecting = false;
        ctx->textInputId = 0;
        ctx->textCursor = 0;
        ctx->textSelectionAnchor = 0;
        ctx->textScrollX = 0.0f;
        ctx->caretBlinkTime = 0.0f;
    }
    ctx->clickConsumed = false;

    Clay_SetLayoutDimensions(layoutSize);
    Clay_SetPointerState((Clay_Vector2){input.mouseX, input.mouseY}, input.pointerDown);
    Clay_UpdateScrollContainers(enableDragScroll, (Clay_Vector2){input.scrollX, input.scrollY}, input.deltaTime);
    Clay_BeginLayout();
}

Clay_RenderCommandArray ClayWidgets_EndFrame(float deltaTime) {
    return Clay_EndLayout(deltaTime);
}

void ClayWidgets_Label(ClayWidgets_Context *ctx, Clay_String text) {
    if (!ctx) {
        return;
    }
    CLAY_TEXT(text, {
        .textColor = ctx->theme.textColor,
        .fontId = ctx->theme.fontBody,
        .fontSize = ctx->theme.fontSizeBody,
    });
}

void ClayWidgets_Heading(ClayWidgets_Context *ctx, Clay_String text) {
    if (!ctx) {
        return;
    }
    CLAY_TEXT(text, {
        .textColor = ctx->theme.textColor,
        .fontId = ctx->theme.fontHeading,
        .fontSize = ctx->theme.fontSizeHeading,
    });
}

void ClayWidgets_Separator(ClayWidgets_Context *ctx) {
    if (!ctx) {
        return;
    }
    CLAY_AUTO_ID({
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_FIXED(1),
            },
        },
        .backgroundColor = ctx->theme.borderColor,
    }) {}
}

bool ClayWidgets_Button(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text) {
    if (!ctx) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool pressedThisFrame = ctx->input.pointerPressed && over;

    if (pressedThisFrame) {
        ctx->activeId = id.id;
    }

    if (!ctx->input.pointerDown && ctx->activeId == id.id) {
        ctx->activeId = 0;
    }

    bool active = ctx->input.pointerDown && ctx->activeId == id.id;
    bool clicked = ClayWidgets__ConsumeClick(ctx, over && (ctx->activeId == id.id || !ctx->input.pointerDown));
    if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
        clicked = true;
    }

    Clay_Color color = ctx->theme.surfaceAltColor;
    if (active) {
        color = ctx->theme.pressedColor;
    } else if (over) {
        color = ctx->theme.hoverColor;
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.md),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = color,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
        .border = {
            .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .textAlignment = CLAY_TEXT_ALIGN_CENTER,
        });
    }

    return clicked;
}

bool ClayWidgets_Checkbox(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value) {
    if (!ctx || !value) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);
    if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
        clicked = true;
    }
    if (clicked) {
        *value = !(*value);
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_FIXED(20),
                    .height = CLAY_SIZING_FIXED(20),
                },
            },
            .backgroundColor = *value ? ctx->theme.accentColor : ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
            .border = {
                .color = (focused || over) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            if (*value) {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_GROW(0),
                        },
                        .padding = CLAY_PADDING_ALL(5),
                    },
                }) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_GROW(0),
                            },
                        },
                        .backgroundColor = (Clay_Color){240, 248, 255, 255},
                        .cornerRadius = CLAY_CORNER_RADIUS(2),
                    }) {}
                }
            }
        }

        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
        });
    }

    return clicked;
}

bool ClayWidgets_Radio(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue
) {
    if (!ctx || !selectedValue) {
        return false;
    }

    bool selected = (*selectedValue == optionValue);
    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);
    if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
        clicked = true;
    }

    if (clicked) {
        *selectedValue = optionValue;
        selected = true;
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(20), .height = CLAY_SIZING_FIXED(20) } },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(10),
            .border = {
                .color = (focused || over) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            if (selected) {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_FIXED(10), .height = CLAY_SIZING_FIXED(10) },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = ctx->theme.accentColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                    .floating = {
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER,
                        },
                        .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                        .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                    },
                }) {}
            }
        }

        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
        });
    }

    return clicked;
}

float ClayWidgets_Slider(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    float value,
    ClayWidgets_SliderOptions options
) {
    if (!ctx) {
        return value;
    }

    float minValue = options.minValue;
    float maxValue = options.maxValue;
    float step = options.step;

    if (maxValue < minValue) {
        float temp = minValue;
        minValue = maxValue;
        maxValue = temp;
    }

    float clamped = ClayWidgets__Clamp(value, minValue, maxValue);
    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);

    if (ctx->input.pointerPressed && over) {
        ctx->activeId = id.id;
    }
    if (!ctx->input.pointerDown && ctx->activeId == id.id) {
        ctx->activeId = 0;
    }

    if (ctx->activeId == id.id && ctx->input.pointerDown) {
        Clay_ElementData data = Clay_GetElementData(id);
        if (data.found && data.boundingBox.width > 0.0f) {
            float localX = ctx->input.mouseX - data.boundingBox.x;
            float ratio = ClayWidgets__Clamp(localX / data.boundingBox.width, 0.0f, 1.0f);
            clamped = minValue + (maxValue - minValue) * ratio;

            if (step > 0.0f) {
                float steps = (clamped - minValue) / step;
                int32_t rounded = (int32_t)(steps + (steps >= 0.0f ? 0.5f : -0.5f));
                clamped = minValue + ((float)rounded * step);
            }
            clamped = ClayWidgets__Clamp(clamped, minValue, maxValue);
        }
    }

    float denominator = (maxValue - minValue);
    float t = denominator > 0.0f ? (clamped - minValue) / denominator : 0.0f;
    t = ClayWidgets__Clamp(t, 0.0f, 1.0f);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(18) },
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(9),
        .border = {
            .color = (focused || over) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_PERCENT(t),
                    .height = CLAY_SIZING_GROW(0),
                },
            },
            .backgroundColor = ClayWidgets__MixColor(ctx->theme.accentMutedColor, ctx->theme.accentColor, t),
            .cornerRadius = CLAY_CORNER_RADIUS(8),
        }) {}
    }

    return clamped;
}

void ClayWidgets_ProgressBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    float progress01,
    Clay_String label
) {
    if (!ctx) {
        return;
    }

    float t = ClayWidgets__Clamp(progress01, 0.0f, 1.0f);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.xs,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    }) {
        if (label.length > 0 && label.chars) {
            CLAY_TEXT(label, {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeSmall,
            });
        }

        CLAY_AUTO_ID({
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(12) },
            },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(6),
            .border = {
                .color = ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = CLAY_SIZING_PERCENT(t), .height = CLAY_SIZING_GROW(0) },
                },
                .backgroundColor = ctx->theme.accentColor,
                .cornerRadius = CLAY_CORNER_RADIUS(6),
            }) {}
        }
    }
}

bool ClayWidgets_TextInput(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    char *buffer,
    int32_t capacity,
    ClayWidgets_TextInputOptions options
) {
    if (!ctx || !buffer || capacity <= 0) {
        return false;
    }

    int32_t length = ClayWidgets__StrLenBounded(buffer, capacity - 1);
    bool changed = false;
    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    const uint16_t fontId = ctx->theme.fontBody;
    const uint16_t fontSize = ctx->theme.fontSizeBody;
    const uint16_t letterSpacing = 0;
    const float fieldHeight = (float)(ctx->theme.fontSizeBody + (int32_t)ctx->theme.spacing.md + 8);
    const float horizontalInset = (float)ctx->theme.spacing.sm;
    const float verticalInset = (float)ctx->theme.spacing.sm;
    Clay_ElementId fieldId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputField"), id.id);
    Clay_ElementId textContentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputContent"), id.id);

    if (ctx->input.pointerPressed && !over && focused) {
        ctx->focusedId = 0;
        focused = false;
    }

    focused = (ctx->focusedId == id.id);

    if (focused && ctx->textInputId != id.id) {
        ClayWidgets__SetCaret(ctx, id.id, length, false);
    }

    ClayWidgets__ClampSelectionToLength(ctx, length);

    Clay_Dimensions lineMetrics = ClayWidgets__MeasureSlice(ctx, "Ag", 2, fontId, fontSize, letterSpacing);
    float textHeight = lineMetrics.height > 0.0f ? lineMetrics.height : (float)fontSize;
    float textOffsetY = verticalInset + ClayWidgets__MaxI32(0, (int32_t)((fieldHeight - verticalInset * 2.0f - textHeight) * 0.5f));
    float availableTextWidth = 0.0f;
    float textScrollX = focused ? ctx->textScrollX : 0.0f;
    float textOffsetX = horizontalInset - textScrollX;
    float pointerTextOffsetX = textOffsetX;

    {
        Clay_ElementData previousFieldData = Clay_GetElementData(fieldId);
        Clay_ElementData previousTextData = Clay_GetElementData(textContentId);
        if (previousFieldData.found) {
            availableTextWidth = previousFieldData.boundingBox.width - horizontalInset * 2.0f;
        }
        if (previousFieldData.found && previousTextData.found) {
            pointerTextOffsetX = previousTextData.boundingBox.x - previousFieldData.boundingBox.x;
            textOffsetY = previousTextData.boundingBox.y - previousFieldData.boundingBox.y;
        }
    }

    if (availableTextWidth <= 0.0f) {
        availableTextWidth = 1.0f;
    }

    /* NOTE: The scroll offset is intentionally NOT updated here. Hit-testing
     * below uses the previous frame's on-screen layout (pointerTextOffsetX for
     * a press, the current textOffsetX for a drag), which reflects what the
     * user actually sees. The scroll is recomputed after all pointer and
     * keyboard input has moved the caret, just before rendering, so the text,
     * caret, and selection highlight stay consistent within a single frame. */

    if (focused && (ctx->input.pointerPressed || (ctx->input.pointerDown && ctx->textPointerSelecting))) {
        Clay_ElementData fieldData = Clay_GetElementData(fieldId);
        if (fieldData.found) {
            float hitTestOffsetX = ctx->input.pointerPressed ? pointerTextOffsetX : textOffsetX;
            float localX = ctx->input.mouseX - (fieldData.boundingBox.x + hitTestOffsetX);
            int32_t cursorFromPointer = ClayWidgets__FindCursorFromLocalX(ctx, buffer, length, localX, fontId, fontSize, letterSpacing);
            if (ctx->input.pointerPressed && over) {
                bool isDoubleClick = ClayWidgets__WasDoubleClick(ctx, id.id);
                ClayWidgets__RecordClick(ctx, id.id);
                if (isDoubleClick && length > 0) {
                    int32_t wordStart = 0;
                    int32_t wordEnd = 0;
                    ClayWidgets__FindWordBounds(buffer, length, cursorFromPointer, &wordStart, &wordEnd);
                    if (wordEnd > wordStart) {
                        ctx->textInputId = id.id;
                        ctx->textSelectionAnchor = wordStart;
                        ctx->textCursor = wordEnd;
                        ctx->textPointerSelecting = false;
                        ctx->caretBlinkTime = 0.0f;
                    } else {
                        ClayWidgets__SetCaret(ctx, id.id, cursorFromPointer, ctx->input.shiftDown);
                        ctx->textPointerSelecting = true;
                    }
                } else {
                    ClayWidgets__SetCaret(ctx, id.id, cursorFromPointer, ctx->input.shiftDown);
                    ctx->textPointerSelecting = true;
                }
            } else if (ctx->input.pointerDown && ctx->textPointerSelecting) {
                ctx->textCursor = cursorFromPointer;
                ctx->caretBlinkTime = 0.0f;
            }
        }
    }

    if (ctx->input.pointerReleased) {
        ctx->textPointerSelecting = false;
    }

    if (focused) {
        if (ctx->input.keySelectAll && length > 0) {
            ctx->textSelectionAnchor = 0;
            ctx->textCursor = length;
            ctx->caretBlinkTime = 0.0f;
        }

        if (ctx->input.keyLeft) {
            int32_t nextCursor = ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor);
            ClayWidgets__SetCaret(ctx, id.id, nextCursor, ctx->input.shiftDown);
        }

        if (ctx->input.keyRight) {
            int32_t nextCursor = ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor);
            ClayWidgets__SetCaret(ctx, id.id, nextCursor, ctx->input.shiftDown);
        }

        if (ctx->input.keyHome) {
            ClayWidgets__SetCaret(ctx, id.id, 0, ctx->input.shiftDown);
        }

        if (ctx->input.keyEnd) {
            ClayWidgets__SetCaret(ctx, id.id, length, ctx->input.shiftDown);
        }

        if (ctx->input.keyBackspace) {
            if (ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            } else if (ctx->textCursor > 0) {
                int32_t deleteFrom = ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor);
                memmove(buffer + deleteFrom, buffer + ctx->textCursor, (size_t)(length - ctx->textCursor + 1));
                length -= (ctx->textCursor - deleteFrom);
                ClayWidgets__SetCaret(ctx, id.id, deleteFrom, false);
                changed = true;
            }
        }

        if (ctx->input.keyDelete) {
            if (ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            } else if (ctx->textCursor < length) {
                int32_t deleteTo = ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor);
                memmove(buffer + ctx->textCursor, buffer + deleteTo, (size_t)(length - deleteTo + 1));
                length -= (deleteTo - ctx->textCursor);
                changed = true;
            }
        }

        if (ctx->input.textUtf8 && ctx->input.textUtf8Length > 0) {
            if (ClayWidgets__InsertTextAtCaret(ctx, id.id, buffer, &length, capacity, ctx->input.textUtf8, ctx->input.textUtf8Length)) {
                changed = true;
            }
        }

        if (ctx->input.keyEnter && options.clearOnEnter && length > 0) {
            buffer[0] = '\0';
            length = 0;
            ClayWidgets__SetCaret(ctx, id.id, 0, false);
            changed = true;
        }

        if (ctx->input.keyEscape) {
            ctx->focusedId = 0;
            ctx->textPointerSelecting = false;
            focused = false;
        }
    }

    /* Update the horizontal scroll now that pointer and keyboard input have
     * finished moving the caret. Doing this after input (rather than before)
     * keeps the rendered text, caret, and selection highlight aligned to the
     * final caret position, preventing the one-frame jump that occurred when
     * clicking into a scrolled (long) field. */
    if (focused) {
        bool pointerSelecting = ctx->input.pointerDown && ctx->textPointerSelecting;
        float pointerLocalX = 0.0f;
        if (pointerSelecting) {
            Clay_ElementData fieldData = Clay_GetElementData(fieldId);
            if (fieldData.found) {
                pointerLocalX = ctx->input.mouseX - (fieldData.boundingBox.x + horizontalInset);
            } else {
                pointerSelecting = false;
            }
        }
        ClayWidgets__UpdateTextScroll(ctx, id.id, buffer, length, availableTextWidth, fontId, fontSize, letterSpacing, pointerSelecting, pointerLocalX);
        textScrollX = ctx->textScrollX;
        textOffsetX = horizontalInset - textScrollX;
    }

    Clay_String displayText;
    if (length > 0) {
        displayText = ClayWidgets__StringFromCString(buffer);
    } else if (options.placeholder) {
        displayText = ClayWidgets__StringFromCString(options.placeholder);
    } else {
        displayText = ClayWidgets__StringFromCString("");
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.xs,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    }) {
        if (label.length > 0 && label.chars) {
            CLAY_TEXT(label, {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeSmall,
            });
        }

        CLAY(fieldId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED((float)(ctx->theme.fontSizeBody + (int32_t)ctx->theme.spacing.md + 8)),
                },
                .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = focused ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
            .border = {
                .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            if (focused && ClayWidgets__HasSelection(ctx) && length > 0) {
                int32_t selectionStart = 0;
                int32_t selectionEnd = 0;
                ClayWidgets__SelectionRange(ctx, &selectionStart, &selectionEnd);
                float selectionX = ClayWidgets__MeasureWidth(ctx, buffer, selectionStart, fontId, fontSize, letterSpacing);
                float selectionWidth = ClayWidgets__MeasureWidth(ctx, buffer, selectionEnd, fontId, fontSize, letterSpacing) - selectionX;
                if (selectionWidth > 0.0f) {
                    CLAY(CLAY_IDI_LOCAL("SelectionHighlight", 0), {
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_FIXED(selectionWidth),
                                .height = CLAY_SIZING_FIXED(textHeight),
                            },
                        },
                        .backgroundColor = ctx->theme.accentMutedColor,
                        .cornerRadius = CLAY_CORNER_RADIUS(3),
                        .floating = {
                            .offset = { .x = textOffsetX + selectionX, .y = textOffsetY },
                            .zIndex = 1,
                            .attachPoints = {
                                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                            },
                            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                            .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                        },
                    }) {}
                }
            }

            CLAY(textContentId, {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIT(0, 0),
                        .height = CLAY_SIZING_FIT(0, 0),
                    },
                },
                .floating = {
                    .offset = { .x = textOffsetX, .y = textOffsetY },
                    .zIndex = 0,
                    .attachPoints = {
                        .element = CLAY_ATTACH_POINT_LEFT_TOP,
                        .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                    },
                    .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                    .attachTo = CLAY_ATTACH_TO_PARENT,
                    .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                },
            }) {
                CLAY_TEXT(displayText, {
                    .textColor = (length > 0) ? ctx->theme.textColor : ctx->theme.textMutedColor,
                    .fontId = fontId,
                    .fontSize = fontSize,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                });
            }

            if (focused && ((int32_t)(ctx->caretBlinkTime * 2.0f) % 2 == 0)) {
                float caretX = ClayWidgets__MeasureWidth(ctx, buffer, ctx->textCursor, fontId, fontSize, letterSpacing);
                CLAY(CLAY_IDI_LOCAL("Caret", 0), {
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_FIXED(1),
                            .height = CLAY_SIZING_FIXED(textHeight),
                        },
                    },
                    .backgroundColor = ctx->theme.textColor,
                    .floating = {
                        .offset = { .x = textOffsetX + caretX, .y = textOffsetY },
                        .zIndex = 2,
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_LEFT_TOP,
                            .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                        },
                        .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                        .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                    },
                }) {}
            }
        }
    }

    return changed;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
