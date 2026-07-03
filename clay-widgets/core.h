#ifndef CLAY_WIDGETS_CORE_H
#define CLAY_WIDGETS_CORE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before core.h"
#endif

void ClayWidgets_Init(ClayWidgets_Context *ctx, ClayWidgets_Theme theme);
void ClayWidgets_SetMeasureTextFunction(ClayWidgets_Context *ctx, ClayWidgets_MeasureTextFunction measureText, void *userData);

void ClayWidgets_BeginFrame(
    ClayWidgets_Context *ctx,
    ClayWidgets_Input input,
    Clay_Dimensions layoutSize,
    bool enableDragScroll
);

Clay_RenderCommandArray ClayWidgets_EndFrame(float deltaTime);

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

#endif

#endif