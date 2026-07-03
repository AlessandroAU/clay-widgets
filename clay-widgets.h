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
    bool keyUp;
    bool keyDown;
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
    uint32_t lastHoveredTextInputId;
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

    uint32_t scrollBarDragContainerId;
    float scrollBarDragStartMouseY;
    float scrollBarDragStartScrollY;

    uint32_t openComboId;
    int32_t comboHighlightIndex;
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

bool ClayWidgets_Combo(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    const Clay_String *items,
    int32_t itemCount,
    int32_t *selectedIndex
);

void ClayWidgets_ScrollBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId
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

static bool ClayWidgets__PointInBoundingBox(float x, float y, Clay_BoundingBox box) {
    return x >= box.x
        && y >= box.y
        && x <= (box.x + box.width)
        && y <= (box.y + box.height);
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

/* Move the caret to an absolute byte offset. When `extend` is false the
 * selection collapses to the caret; when true the anchor is left in place so
 * the selection grows/shrinks. Always resets the blink so the caret is visible
 * immediately after it moves. The focused input already owns the shared caret
 * state (see the focus-init block in ClayWidgets_TextInput), so this only
 * touches cursor/anchor/blink. */
static void ClayWidgets__MoveCaret(ClayWidgets_Context *ctx, int32_t offset, bool extend) {
    if (!ctx) {
        return;
    }
    ctx->textCursor = offset;
    if (!extend) {
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

/* Adjust the horizontal scroll so the caret stays inside the visible content
 * area. This is the single scroll rule: for an in-bounds pointer the caret it
 * produces is already visible, so no scroll change happens and the caret cannot
 * drift; dragging past an edge (or moving the caret with the keyboard past the
 * edge) scrolls just enough to reveal it. */
static void ClayWidgets__UpdateTextScroll(
    ClayWidgets_Context *ctx,
    uint32_t id,
    const char *text,
    int32_t length,
    float availableWidth,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
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
    float caretX = ClayWidgets__MeasureWidth(ctx, text, ctx->textCursor, fontId, fontSize, letterSpacing);
    float scrollX = ClayWidgets__ClampF32(ctx->textScrollX, 0.0f, maxScroll);

    if (caretX < scrollX) {
        scrollX = caretX;
    } else if (caretX > scrollX + availableWidth) {
        scrollX = caretX - availableWidth;
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
    ClayWidgets__MoveCaret(ctx, start, false);
    return true;
}

static bool ClayWidgets__InsertTextAtCaret(
    ClayWidgets_Context *ctx,
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
    ClayWidgets__MoveCaret(ctx, insertAt + toCopy, false);
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

    Clay_Vector2 pointerPosition = { input.mouseX, input.mouseY };
    Clay_Vector2 scrollDelta = { input.scrollX, input.scrollY };
    Clay_SetPointerState(pointerPosition, input.pointerDown);

    /* A text input uses clip+childOffset to implement horizontal text scrolling.
     * Clay treats clipped elements as scroll containers, which can steal wheel
     * routing from the outer panel. When wheel-scrolling over the active text
     * field, temporarily route scroll hit-testing to just outside the field so
     * the parent scroll panel receives vertical wheel input. */
    bool rerouteWheelToParent = false;
    uint32_t wheelTargetTextInputId = ctx->textInputId != 0 ? ctx->textInputId : ctx->lastHoveredTextInputId;
    if (scrollDelta.y != 0.0f && wheelTargetTextInputId != 0) {
        Clay_ElementId activeFieldId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputField"), wheelTargetTextInputId);
        Clay_ElementData activeFieldData = Clay_GetElementData(activeFieldId);
        if (activeFieldData.found && ClayWidgets__PointInBoundingBox(pointerPosition.x, pointerPosition.y, activeFieldData.boundingBox)) {
            rerouteWheelToParent = true;
            Clay_Vector2 reroutedPointer = pointerPosition;
            reroutedPointer.y = activeFieldData.boundingBox.y - 1.0f;
            if (reroutedPointer.y < 0.0f) {
                reroutedPointer.y = 0.0f;
            }
            Clay_SetPointerState(reroutedPointer, input.pointerDown);
        }
    }

    Clay_UpdateScrollContainers(enableDragScroll, scrollDelta, input.deltaTime);

    if (rerouteWheelToParent) {
        Clay_SetPointerState(pointerPosition, input.pointerDown);
    }

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
                .padding = CLAY_PADDING_ALL(1),
            },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
            .clip = { .horizontal = true, .vertical = true },
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
                        .padding = CLAY_PADDING_ALL(3),
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = ctx->theme.accentColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm > 0 ? ctx->theme.radiusSm - 1 : 0),
                    .clip = { .horizontal = true, .vertical = true },
                }) {
                    CLAY_TEXT(CLAY_STRING("X"), {
                        .textColor = (Clay_Color){240, 248, 255, 255},
                        .fontId = ctx->theme.fontBody,
                        .fontSize = 14,
                    });
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
                .padding = CLAY_PADDING_ALL(1),
            },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(6),
            .clip = { .horizontal = true, .vertical = true },
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
                .cornerRadius = CLAY_CORNER_RADIUS(5),
            }) {}
        }
    }
}

/* Convert a screen-space mouse X into a byte offset in `buffer`. `contentOriginX`
 * is the screen X of the first glyph when unscrolled; `scrollX` is the current
 * horizontal scroll. Both the press and drag paths use the *same* reference,
 * which is what keeps the caret from drifting under a stationary cursor. */
static int32_t ClayWidgets__CursorFromMouse(
    ClayWidgets_Context *ctx,
    const char *buffer,
    int32_t length,
    float mouseX,
    float contentOriginX,
    float scrollX,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
) {
    float localX = (mouseX - contentOriginX) + scrollX;
    return ClayWidgets__FindCursorFromLocalX(ctx, buffer, length, localX, fontId, fontSize, letterSpacing);
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

    const uint16_t fontId = ctx->theme.fontBody;
    const uint16_t fontSize = ctx->theme.fontSizeBody;
    const uint16_t letterSpacing = 0;
    const float horizontalInset = (float)ctx->theme.spacing.sm;
    const float fieldHeight = (float)(ctx->theme.fontSizeBody + (int32_t)ctx->theme.spacing.md + 8);
    Clay_ElementId fieldId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputField"), id.id);
    Clay_ElementId textContentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputContent"), id.id);

    /* ------------------------------------------------------------------ */
    /* Focus                                                              */
    /* ------------------------------------------------------------------ */
    bool over = Clay_PointerOver(id);
    if (over) {
        ctx->lastHoveredTextInputId = id.id;
    }
    ClayWidgets__RegisterFocusable(ctx, id, over); /* focuses on press-over, feeds Tab order */
    if (ctx->input.pointerPressed && !over && ctx->focusedId == id.id) {
        ctx->focusedId = 0; /* clicking elsewhere blurs */
    }
    bool focused = (ctx->focusedId == id.id);

    /* First frame of focus: park the caret at the end, no selection. */
    if (focused && ctx->textInputId != id.id) {
        ctx->textInputId = id.id;
        ctx->textCursor = length;
        ctx->textSelectionAnchor = length;
        ctx->textScrollX = 0.0f;
        ctx->textPointerSelecting = false;
        ctx->caretBlinkTime = 0.0f;
    }
    /* The caret/selection are shared context state owned by whichever input is
     * focused. Only clamp against THIS field's length when this field owns the
     * caret, otherwise an unfocused (and possibly shorter) field would drag the
     * focused field's caret back to its own length. */
    if (ctx->textInputId == id.id) {
        ClayWidgets__ClampSelectionToLength(ctx, length);
    }

    /* ------------------------------------------------------------------ */
    /* Geometry (from the previous frame's resolved layout)               */
    /* ------------------------------------------------------------------ */
    Clay_ElementData fieldData = Clay_GetElementData(fieldId);
    float availableWidth = fieldData.found ? (fieldData.boundingBox.width - horizontalInset * 2.0f) : 0.0f;
    if (availableWidth <= 0.0f) {
        availableWidth = 1.0f;
    }
    /* Screen X of the first glyph when the text is not scrolled. The visible
     * glyph at scroll S sits at contentOriginX - S. */
    float contentOriginX = fieldData.found ? (fieldData.boundingBox.x + horizontalInset) : 0.0f;

    /* ------------------------------------------------------------------ */
    /* Pointer selection                                                  */
    /* ------------------------------------------------------------------ */
    if (focused && fieldData.found) {
        if (ctx->input.pointerPressed && over) {
            int32_t hit = ClayWidgets__CursorFromMouse(ctx, buffer, length, ctx->input.mouseX, contentOriginX, ctx->textScrollX, fontId, fontSize, letterSpacing);
            bool isDoubleClick = ClayWidgets__WasDoubleClick(ctx, id.id);
            ClayWidgets__RecordClick(ctx, id.id);

            int32_t wordStart = 0;
            int32_t wordEnd = 0;
            if (isDoubleClick && length > 0) {
                ClayWidgets__FindWordBounds(buffer, length, hit, &wordStart, &wordEnd);
            }

            if (wordEnd > wordStart) {
                /* Double-click landed on a word: select it, no drag. */
                ctx->textSelectionAnchor = wordStart;
                ctx->textCursor = wordEnd;
                ctx->textPointerSelecting = false;
                ctx->caretBlinkTime = 0.0f;
            } else {
                /* Single click (or shift-click to extend): place/extend caret
                 * and begin a drag-select. */
                ClayWidgets__MoveCaret(ctx, hit, ctx->input.shiftDown);
                ctx->textPointerSelecting = true;
            }
        } else if (ctx->input.pointerDown && ctx->textPointerSelecting) {
            int32_t hit = ClayWidgets__CursorFromMouse(ctx, buffer, length, ctx->input.mouseX, contentOriginX, ctx->textScrollX, fontId, fontSize, letterSpacing);
            ClayWidgets__MoveCaret(ctx, hit, true); /* extend selection while dragging */
        }
    }
    if (ctx->input.pointerReleased) {
        ctx->textPointerSelecting = false;
    }

    /* ------------------------------------------------------------------ */
    /* Keyboard                                                           */
    /* ------------------------------------------------------------------ */
    if (focused) {
        bool extend = ctx->input.shiftDown;
        bool hadSelection = ClayWidgets__HasSelection(ctx);
        int32_t selStart = 0;
        int32_t selEnd = 0;
        ClayWidgets__SelectionRange(ctx, &selStart, &selEnd);

        if (ctx->input.keySelectAll && length > 0) {
            ctx->textSelectionAnchor = 0;
            ctx->textCursor = length;
            ctx->caretBlinkTime = 0.0f;
        }

        if (ctx->input.keyLeft) {
            /* Collapse to the left edge of an existing selection, else step. */
            int32_t target = (hadSelection && !extend)
                ? selStart
                : ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor);
            ClayWidgets__MoveCaret(ctx, target, extend);
        }

        if (ctx->input.keyRight) {
            int32_t target = (hadSelection && !extend)
                ? selEnd
                : ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor);
            ClayWidgets__MoveCaret(ctx, target, extend);
        }

        if (ctx->input.keyHome) {
            ClayWidgets__MoveCaret(ctx, 0, extend);
        }

        if (ctx->input.keyEnd) {
            ClayWidgets__MoveCaret(ctx, length, extend);
        }

        if (ctx->input.keyBackspace) {
            if (ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            } else if (ctx->textCursor > 0) {
                int32_t from = ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor);
                memmove(buffer + from, buffer + ctx->textCursor, (size_t)(length - ctx->textCursor + 1));
                length -= (ctx->textCursor - from);
                ClayWidgets__MoveCaret(ctx, from, false);
                changed = true;
            }
        }

        if (ctx->input.keyDelete) {
            if (ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            } else if (ctx->textCursor < length) {
                int32_t to = ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor);
                memmove(buffer + ctx->textCursor, buffer + to, (size_t)(length - to + 1));
                length -= (to - ctx->textCursor);
                ctx->caretBlinkTime = 0.0f;
                changed = true;
            }
        }

        if (ctx->input.textUtf8 && ctx->input.textUtf8Length > 0) {
            if (ClayWidgets__InsertTextAtCaret(ctx, buffer, &length, capacity, ctx->input.textUtf8, ctx->input.textUtf8Length)) {
                changed = true;
            }
        }

        if (ctx->input.keyEnter && options.clearOnEnter && length > 0) {
            buffer[0] = '\0';
            length = 0;
            ClayWidgets__MoveCaret(ctx, 0, false);
            changed = true;
        }

        if (ctx->input.keyEscape) {
            ctx->focusedId = 0;
            ctx->textPointerSelecting = false;
            focused = false;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Scroll so the caret is visible (only after all caret movement)     */
    /* ------------------------------------------------------------------ */
    if (focused) {
        ClayWidgets__UpdateTextScroll(ctx, id.id, buffer, length, availableWidth, fontId, fontSize, letterSpacing);
    }

    float textScrollX = focused ? ctx->textScrollX : 0.0f;
    float textOffsetX = horizontalInset - textScrollX;

    Clay_Dimensions lineMetrics = ClayWidgets__MeasureSlice(ctx, "Ag", 2, fontId, fontSize, letterSpacing);
    float textHeight = lineMetrics.height > 0.0f ? lineMetrics.height : (float)fontSize;
    float textOffsetY = (fieldHeight - textHeight) * 0.5f;
    if (textOffsetY < 0.0f) {
        textOffsetY = 0.0f;
    }

    Clay_String displayText;
    if (length > 0) {
        displayText = ClayWidgets__StringFromCString(buffer);
    } else if (options.placeholder) {
        displayText = ClayWidgets__StringFromCString(options.placeholder);
    } else {
        displayText = ClayWidgets__StringFromCString("");
    }

    /* ------------------------------------------------------------------ */
    /* Render: field with floating selection (z0), text (z1), caret (z2). */
    /* All three are offset by the same textOffsetX so they stay aligned. */
    /* ------------------------------------------------------------------ */
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
                    .height = CLAY_SIZING_FIXED(fieldHeight),
                },
                .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = focused ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
            .clip = { .horizontal = true, .vertical = true, .childOffset = { -textScrollX, 0.0f } },
            .border = {
                .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            /* Render text, selection, caret as regular children that respect clipping */
            if (focused && ClayWidgets__HasSelection(ctx) && length > 0) {
                int32_t selStart = 0;
                int32_t selEnd = 0;
                ClayWidgets__SelectionRange(ctx, &selStart, &selEnd);
                float startX = ClayWidgets__MeasureWidth(ctx, buffer, selStart, fontId, fontSize, letterSpacing);
                float endX = ClayWidgets__MeasureWidth(ctx, buffer, selEnd, fontId, fontSize, letterSpacing);
                float selWidth = endX - startX;
                if (selWidth > 0.0f) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_FIXED(selWidth),
                                .height = CLAY_SIZING_FIXED(textHeight),
                            },
                            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = ctx->theme.accentMutedColor,
                        .cornerRadius = CLAY_CORNER_RADIUS(3),
                    }) {}
                }
            }

            CLAY_TEXT(displayText, {
                .textColor = (length > 0) ? ctx->theme.textColor : ctx->theme.textMutedColor,
                .fontId = fontId,
                .fontSize = fontSize,
                .wrapMode = CLAY_TEXT_WRAP_NONE,
            });

            if (focused && ((int32_t)(ctx->caretBlinkTime * 2.0f) % 2 == 0)) {
                float caretX = ClayWidgets__MeasureWidth(ctx, buffer, ctx->textCursor, fontId, fontSize, letterSpacing);
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_FIXED(1),
                            .height = CLAY_SIZING_FIXED(textHeight),
                        },
                    },
                    .backgroundColor = ctx->theme.textColor,
                }) {}
            }
        }
    }

    return changed;
}

bool ClayWidgets_Combo(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    const Clay_String *items,
    int32_t itemCount,
    int32_t *selectedIndex
) {
    if (!ctx || !items || itemCount <= 0 || !selectedIndex) {
        return false;
    }

    bool changed = false;
    Clay_ElementId triggerId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboTrigger"), id.id);
    Clay_ElementId dropdownId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboDropdown"), id.id);

    bool triggerOver = Clay_PointerOver(triggerId);
    bool anyOver = Clay_PointerOver(id) || triggerOver || Clay_PointerOver(dropdownId);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, anyOver);
    bool isOpen = (ctx->openComboId == id.id);

    /* Close if focus moved to a different widget */
    if (isOpen && !focused && ctx->focusedId != 0) {
        ctx->openComboId = 0;
        isOpen = false;
    }

    /* Escape closes the dropdown */
    if (isOpen && focused && ctx->input.keyEscape) {
        ctx->openComboId = 0;
        isOpen = false;
    }

    /* Close if pointer pressed outside the trigger and dropdown */
    if (isOpen && ctx->input.pointerPressed) {
        if (!Clay_PointerOver(triggerId) && !Clay_PointerOver(dropdownId)) {
            ctx->openComboId = 0;
            isOpen = false;
        }
    }

    /* Toggle open on trigger click */
    if (ClayWidgets__ConsumeClick(ctx, triggerOver)) {
        if (isOpen) {
            ctx->openComboId = 0;
            isOpen = false;
        } else {
            ctx->openComboId = id.id;
            ctx->comboHighlightIndex = (*selectedIndex >= 0 && *selectedIndex < itemCount)
                ? *selectedIndex : 0;
            isOpen = true;
        }
    }

    /* Open with Enter key when focused and closed */
    if (focused && !isOpen && ClayWidgets__ActivateFocused(ctx, id)) {
        ctx->openComboId = id.id;
        ctx->comboHighlightIndex = (*selectedIndex >= 0 && *selectedIndex < itemCount)
            ? *selectedIndex : 0;
        isOpen = true;
    }

    /* Keyboard navigation inside open dropdown */
    if (focused && isOpen) {
        if (ctx->input.keyUp) {
            ctx->comboHighlightIndex = ctx->comboHighlightIndex > 0
                ? ctx->comboHighlightIndex - 1 : itemCount - 1;
        }
        if (ctx->input.keyDown) {
            ctx->comboHighlightIndex = ctx->comboHighlightIndex < itemCount - 1
                ? ctx->comboHighlightIndex + 1 : 0;
        }
        if (ctx->input.keyEnter) {
            if (ctx->comboHighlightIndex >= 0 && ctx->comboHighlightIndex < itemCount) {
                *selectedIndex = ctx->comboHighlightIndex;
                changed = true;
            }
            ctx->openComboId = 0;
            isOpen = false;
        }
    }

    /* Item click selection */
    if (isOpen) {
        for (int32_t i = 0; i < itemCount; i++) {
            Clay_ElementId itemId = Clay_GetElementIdWithIndex(
                CLAY_STRING("ClayWidgetsComboItem"),
                (uint32_t)((uint64_t)id.id * 1000003u + (uint32_t)i)
            );
            bool itemOver = Clay_PointerOver(itemId);
            if (itemOver) {
                ctx->comboHighlightIndex = i;
            }
            if (ClayWidgets__ConsumeClick(ctx, itemOver)) {
                *selectedIndex = i;
                ctx->openComboId = 0;
                isOpen = false;
                changed = true;
                break;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* Geometry from previous frame                                        */
    /* ------------------------------------------------------------------ */
    Clay_ElementData triggerData = Clay_GetElementData(triggerId);
    float dropdownMinWidth = triggerData.found ? triggerData.boundingBox.width : 0.0f;

    const float fieldHeight = (float)(ctx->theme.fontSizeBody + (int32_t)ctx->theme.spacing.md + 8);
    float r = (float)ctx->theme.radiusSm;

    Clay_String displayText = (*selectedIndex >= 0 && *selectedIndex < itemCount)
        ? items[*selectedIndex]
        : CLAY_STRING("Select...");

    Clay_Color triggerBg = ctx->theme.surfaceAltColor;
    if (triggerOver || isOpen) {
        triggerBg = ctx->theme.hoverColor;
    }

    /* ------------------------------------------------------------------ */
    /* Render                                                              */
    /* ------------------------------------------------------------------ */
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

        CLAY(triggerId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(fieldHeight),
                },
                .padding = (Clay_Padding){
                    .left = ctx->theme.spacing.sm,
                    .right = ctx->theme.spacing.sm,
                    .top = 0,
                    .bottom = 0,
                },
                .childGap = ctx->theme.spacing.sm,
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = triggerBg,
            .cornerRadius = isOpen
                ? (Clay_CornerRadius){ r, r, 0.0f, 0.0f }
                : (Clay_CornerRadius){ r, r, r, r },
            .border = {
                .color = (focused || isOpen) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                },
            }) {
                CLAY_TEXT(displayText, {
                    .textColor = ctx->theme.textColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeBody,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                });
            }
            CLAY_TEXT(CLAY_STRING("v"), {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }

        /* Dropdown popup (floats below the trigger) */
        if (isOpen) {
            CLAY(dropdownId, {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIT(dropdownMinWidth, 0),
                        .height = CLAY_SIZING_FIT(0, 0),
                    },
                    .childGap = 0,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = ctx->theme.surfaceAltColor,
                .cornerRadius = (Clay_CornerRadius){ 0.0f, 0.0f, r, r },
                .floating = {
                    .parentId = triggerId.id,
                    .zIndex = 10,
                    .attachPoints = {
                        .element = CLAY_ATTACH_POINT_LEFT_TOP,
                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM,
                    },
                    .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
                },
                .border = {
                    .color = ctx->theme.focusRingColor,
                    .width = { .left = 1, .right = 1, .top = 0, .bottom = 1 },
                },
            }) {
                for (int32_t i = 0; i < itemCount; i++) {
                    Clay_ElementId itemId = Clay_GetElementIdWithIndex(
                        CLAY_STRING("ClayWidgetsComboItem"),
                        (uint32_t)((uint64_t)id.id * 1000003u + (uint32_t)i)
                    );
                    bool itemOver = Clay_PointerOver(itemId);
                    bool isHighlighted = (i == ctx->comboHighlightIndex);
                    bool isSelected = (i == *selectedIndex);

                    Clay_Color itemBg = ctx->theme.surfaceAltColor;
                    if (itemOver || isHighlighted) {
                        itemBg = ctx->theme.hoverColor;
                    } else if (isSelected) {
                        itemBg = ctx->theme.accentMutedColor;
                    }

                    CLAY(itemId, {
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT(0, 0),
                            },
                            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
                            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = itemBg,
                    }) {
                        CLAY_TEXT(items[i], {
                            .textColor = ctx->theme.textColor,
                            .fontId = ctx->theme.fontBody,
                            .fontSize = ctx->theme.fontSizeBody,
                            .wrapMode = CLAY_TEXT_WRAP_NONE,
                        });
                    }
                }
            }
        }
    }

    return changed;
}

void ClayWidgets_ScrollBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId
) {
    if (!ctx) {
        return;
    }

    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(scrollContainerId);
    if (!scrollData.found || !scrollData.scrollPosition) {
        return;
    }

    float containerHeight = scrollData.scrollContainerDimensions.height;
    float contentHeight = scrollData.contentDimensions.height;

    if (containerHeight <= 0.0f || contentHeight <= 0.0f) {
        return;
    }

    /* Calculate scroll bar dimensions */
    float scrollableHeight = contentHeight - containerHeight;
    if (scrollableHeight <= 0.0f) {
        return; /* No scrolling needed */
    }

    const float trackWidth = 8.0f;
    const float trackPadding = 1.0f;
    float trackInnerHeight = containerHeight - trackPadding * 2.0f;
    if (trackInnerHeight <= 1.0f) {
        return;
    }

    float scrollProgress = ClayWidgets__Clamp((-scrollData.scrollPosition->y) / scrollableHeight, 0.0f, 1.0f);
    float scrollBarHeight = (containerHeight / contentHeight) * trackInnerHeight;
    scrollBarHeight = ClayWidgets__Clamp(scrollBarHeight, 10.0f, trackInnerHeight);
    float thumbTravel = trackInnerHeight - scrollBarHeight;
    float scrollBarY = scrollProgress * thumbTravel;

    /* Render scrollbar as bordered track + inner thumb to avoid 1px overflow. */
    Clay_ElementId scrollBarTrackId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarTrack"), scrollContainerId.id);
    Clay_ElementId scrollBarId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarThumb"), scrollContainerId.id);
    bool overThumb = Clay_PointerOver(scrollBarId);

    if (ctx->input.pointerPressed && overThumb) {
        ctx->activeId = scrollBarId.id;
        ctx->scrollBarDragContainerId = scrollContainerId.id;
        ctx->scrollBarDragStartMouseY = ctx->input.mouseY;
        ctx->scrollBarDragStartScrollY = scrollData.scrollPosition->y;
    }

    bool draggingThumb = ctx->input.pointerDown
        && ctx->activeId == scrollBarId.id
        && ctx->scrollBarDragContainerId == scrollContainerId.id;

    if (draggingThumb) {
        if (thumbTravel > 0.0f) {
            float mouseDeltaY = ctx->input.mouseY - ctx->scrollBarDragStartMouseY;
            float newScrollY = ctx->scrollBarDragStartScrollY - (mouseDeltaY / thumbTravel) * scrollableHeight;
            scrollData.scrollPosition->y = ClayWidgets__Clamp(newScrollY, -scrollableHeight, 0.0f);

            scrollProgress = ClayWidgets__Clamp((-scrollData.scrollPosition->y) / scrollableHeight, 0.0f, 1.0f);
            scrollBarY = scrollProgress * thumbTravel;
        }
    }

    if (ctx->input.pointerReleased && ctx->activeId == scrollBarId.id) {
        ctx->activeId = 0;
        ctx->scrollBarDragContainerId = 0;
        ctx->scrollBarDragStartMouseY = 0.0f;
        ctx->scrollBarDragStartScrollY = 0.0f;
    }

    CLAY(scrollBarTrackId, {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(trackWidth),
                .height = CLAY_SIZING_FIXED(containerHeight),
            },
            .padding = CLAY_PADDING_ALL((uint16_t)trackPadding),
            .childGap = 0,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .floating = {
            .offset = { .x = 3.0f, .y = 0.0f },
            .parentId = scrollContainerId.id,
            .zIndex = 100,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_RIGHT_TOP,
                .parent = CLAY_ATTACH_POINT_RIGHT_TOP,
            },
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
        },
        .clip = { .horizontal = true, .vertical = true },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        if (scrollBarY > 0.0f) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_GROW(0),
                        .height = CLAY_SIZING_FIXED(scrollBarY),
                    },
                },
            }) {}
        }

        CLAY(scrollBarId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(scrollBarHeight),
                },
            },
            .backgroundColor = draggingThumb ? ctx->theme.accentColor : (overThumb ? ctx->theme.accentMutedColor : ctx->theme.borderColor),
            .cornerRadius = CLAY_CORNER_RADIUS(3),
        }) {}
    }
}

#endif

#ifdef __cplusplus
}
#endif

#endif
