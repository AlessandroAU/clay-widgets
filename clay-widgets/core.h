#ifndef CLAY_WIDGETS_CORE_H
#define CLAY_WIDGETS_CORE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before core.h"
#endif

void ClayWidgets_Init(ClayWidgets_Context *ctx, ClayWidgets_Theme theme);
void ClayWidgets_SetMeasureTextFunction(ClayWidgets_Context *ctx, ClayWidgets_MeasureTextFunction measureText, void *userData);

// Optional: wire the platform clipboard (e.g. raylib's Get/SetClipboardText)
// so a focused text input supports copy/cut/paste. Without it those keys are
// ignored.
void ClayWidgets_SetClipboardFunctions(
    ClayWidgets_Context *ctx,
    ClayWidgets_GetClipboardTextFunction getText,
    ClayWidgets_SetClipboardTextFunction setText,
    void *userData
);

// Optional: receive a message when a compile-time cap is exceeded at runtime
// (scratch strings, focus order, table columns), once per category per frame.
// With no handler installed, the CLAY_WIDGETS_ASSERT backstop fires instead.
void ClayWidgets_SetErrorHandler(ClayWidgets_Context *ctx, ClayWidgets_ErrorHandlerFunction handler, void *userData);

void ClayWidgets_BeginFrame(
    ClayWidgets_Context *ctx,
    ClayWidgets_Input input,
    Clay_Dimensions layoutSize,
    bool enableDragScroll
);

Clay_RenderCommandArray ClayWidgets_EndFrame(ClayWidgets_Context *ctx);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

#include <math.h>
#include <string.h>

// Overflow categories for ClayWidgets__ReportError; one report per category
// per frame so a persistent overflow doesn't spam the handler.
#define CLAY_WIDGETS__ERROR_FLAG_SCRATCH    (1u << 0)
#define CLAY_WIDGETS__ERROR_FLAG_FOCUSABLES (1u << 1)
#define CLAY_WIDGETS__ERROR_FLAG_TABLE_COLS (1u << 2)

static void ClayWidgets__ReportError(ClayWidgets_Context *ctx, uint32_t flag, const char *message) {
    if (!ctx || (ctx->frameErrorFlags & flag)) {
        return;
    }
    ctx->frameErrorFlags |= flag;
    if (ctx->errorHandler) {
        ctx->errorHandler(message, ctx->errorHandlerUserData);
        return;
    }
    CLAY_WIDGETS_ASSERT(message);
}

// The only places the library touches Clay's internal (double-underscore)
// API: the open/configure/close triple that Begin/End style widgets need
// because the CLAY() macro's block scoping can't span two function calls, and
// the seeded string hash behind CLAY_SIDI_LOCAL. Isolated here so a Clay
// upgrade that changes internals is a one-file fix.
//
// NOTE: the declaration argument is evaluated BEFORE the element opens (it's
// a function argument), unlike the CLAY() macro where the struct is built
// after Clay__OpenElementWithId runs. Never call open-element-sensitive
// functions like Clay_GetScrollOffset() inside the declaration passed here -
// they would read the parent. For scroll containers use
// ClayWidgets__BeginScrollElement, which stamps the offset after opening.
static void ClayWidgets__BeginElement(Clay_ElementId id, Clay_ElementDeclaration declaration) {
    Clay__OpenElementWithId(id);
    Clay__ConfigureOpenElementPtr(&declaration);
}

// BeginElement for scroll containers: opens the element first, then fills
// clip.childOffset from the element's own retained scroll offset.
// Clay_GetScrollOffset() reads the *currently open* element, so it must run
// after the open - calling it in the declaration argument would read the
// parent and freeze the content at offset zero.
static void ClayWidgets__BeginScrollElement(Clay_ElementId id, Clay_ElementDeclaration declaration) {
    Clay__OpenElementWithId(id);
    declaration.clip.childOffset = Clay_GetScrollOffset();
    Clay__ConfigureOpenElementPtr(&declaration);
}

static void ClayWidgets__EndElement(void) {
    Clay__CloseElement();
}

// Derives a per-item child id from a widget's own id (used as the hash seed)
// and an item index - the same mechanism as CLAY_SIDI_LOCAL, so items of two
// instances of the same widget can't collide by construction.
static Clay_ElementId ClayWidgets__ChildId(Clay_ElementId parent, Clay_String label, int32_t index) {
    return Clay__HashStringWithOffset(label, (uint32_t)index, parent.id);
}

// One-line input field height (text input, combo trigger): the body font plus
// vertical breathing room. Shared so mixed rows of fields line up exactly.
static float ClayWidgets__FieldHeight(const ClayWidgets_Context *ctx) {
    return (float)(ctx->theme.fontSizeBody + (int32_t)ctx->theme.spacing.md + 8);
}

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

// Claims the next scratch buffer for a dynamic per-frame string. Clay holds
// the returned pointer until render, so claiming more than
// CLAY_WIDGETS_TEXT_SCRATCH_COUNT buffers in one frame reuses a live one and
// corrupts earlier widgets' text - report that loudly instead of rendering
// garbage silently.
static char *ClayWidgets__ClaimScratch(ClayWidgets_Context *ctx) {
    if (ctx->textScratchNext >= CLAY_WIDGETS_TEXT_SCRATCH_COUNT) {
        ClayWidgets__ReportError(ctx, CLAY_WIDGETS__ERROR_FLAG_SCRATCH,
            "text scratch pool exhausted: more than CLAY_WIDGETS_TEXT_SCRATCH_COUNT dynamic "
            "strings (stepper/slider values) in one frame; earlier values will render corrupted. "
            "Define CLAY_WIDGETS_TEXT_SCRATCH_COUNT larger.");
    }
    char *buf = ctx->textScratch[ctx->textScratchNext % CLAY_WIDGETS_TEXT_SCRATCH_COUNT];
    ctx->textScratchNext++;
    return buf;
}

// Formats an integer into one of the context's scratch buffers and returns a
// Clay_String pointing at it. Valid until the buffer is reused (see
// ClayWidgets__ClaimScratch).
static Clay_String ClayWidgets__ScratchInt(ClayWidgets_Context *ctx, int32_t value) {
    Clay_String result = {0};
    if (!ctx) {
        result.chars = "";
        result.isStaticallyAllocated = true;
        return result;
    }

    char *buf = ClayWidgets__ClaimScratch(ctx);

    char digits[16];
    int32_t digitCount = 0;
    uint32_t magnitude;
    bool negative = value < 0;
    if (negative) {
        magnitude = (uint32_t)(-(int64_t)value);
    } else {
        magnitude = (uint32_t)value;
    }
    if (magnitude == 0) {
        digits[digitCount++] = '0';
    }
    while (magnitude > 0 && digitCount < (int32_t)sizeof(digits)) {
        digits[digitCount++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    }

    int32_t length = 0;
    if (negative) {
        buf[length++] = '-';
    }
    for (int32_t i = digitCount - 1; i >= 0; --i) {
        buf[length++] = digits[i];
    }
    buf[length] = '\0';

    result.chars = buf;
    result.length = length;
    result.isStaticallyAllocated = false;
    return result;
}

// Formats a float into one of the context's scratch buffers with `decimals`
// fractional digits (rounded), returning a Clay_String pointing at it. Valid
// until the buffer is reused (see ClayWidgets__ClaimScratch). stdio-free,
// like ClayWidgets__ScratchInt.
static Clay_String ClayWidgets__ScratchFloat(ClayWidgets_Context *ctx, float value, int32_t decimals) {
    Clay_String result = {0};
    if (!ctx) {
        result.chars = "";
        result.isStaticallyAllocated = true;
        return result;
    }
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;

    char *buf = ClayWidgets__ClaimScratch(ctx);
    const int32_t cap = (int32_t)sizeof(ctx->textScratch[0]) - 1; // leave room for NUL

    bool negative = value < 0.0f;
    double mag = negative ? -(double)value : (double)value;

    uint64_t scale = 1;
    for (int32_t i = 0; i < decimals; ++i) scale *= 10u;

    // Round to `decimals` places, then split into whole and fractional parts.
    uint64_t scaled = (uint64_t)(mag * (double)scale + 0.5);
    uint64_t intPart = scaled / scale;
    uint64_t fracPart = scaled % scale;

    char digits[20];
    int32_t digitCount = 0;
    if (intPart == 0) {
        digits[digitCount++] = '0';
    }
    while (intPart > 0 && digitCount < (int32_t)sizeof(digits)) {
        digits[digitCount++] = (char)('0' + (int)(intPart % 10u));
        intPart /= 10u;
    }

    int32_t length = 0;
    if (negative && length < cap) {
        buf[length++] = '-';
    }
    for (int32_t i = digitCount - 1; i >= 0 && length < cap; --i) {
        buf[length++] = digits[i];
    }
    if (decimals > 0 && length < cap) {
        buf[length++] = '.';
        // Emit fractional digits most-significant first, zero-padded to `decimals`.
        uint64_t divisor = scale / 10u;
        for (int32_t i = 0; i < decimals && length < cap; ++i) {
            uint64_t d = divisor > 0 ? (fracPart / divisor) % 10u : 0u;
            buf[length++] = (char)('0' + (int)d);
            if (divisor > 0) divisor /= 10u;
        }
    }
    buf[length] = '\0';

    result.chars = buf;
    result.length = length;
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

#ifndef CLAY_WIDGETS_ANIM_HOVER_DURATION
#define CLAY_WIDGETS_ANIM_HOVER_DURATION 0.12f
#endif

#ifndef CLAY_WIDGETS_ANIM_ENTER_DURATION
#define CLAY_WIDGETS_ANIM_ENTER_DURATION 0.18f
#endif

// Enter-transition initial state: start fully transparent so an appearing
// element fades its own fill in from alpha 0 to its target color. Only the
// element's own rectangle fades - Clay transitions do not cascade to child
// elements - so this is used on solid overlays (a modal scrim) whose visual is
// just their own fill, not on panels that host crisp child text.
static Clay_TransitionData ClayWidgets__FadeInInitialState(Clay_TransitionData targetState, Clay_TransitionProperty properties) {
    (void)properties;
    targetState.backgroundColor.a = 0;
    return targetState;
}

// Fade-in transition for a modal scrim: its dim overlay eases from transparent
// to its target alpha when the modal opens, while the dialog on top stays crisp.
// ALLOW_INTERACTIONS keeps the dialog clickable during the fade (the default
// would disable the subtree's input while entering). Zeroed when animations are
// off, so the scrim snaps to full dim.
static Clay_TransitionElementConfig ClayWidgets__ScrimFadeIn(const ClayWidgets_Context *ctx) {
    Clay_TransitionElementConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (ctx && ctx->animationsEnabled) {
        cfg.handler = Clay_EaseOut;
        cfg.duration = CLAY_WIDGETS_ANIM_ENTER_DURATION;
        cfg.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
        cfg.interactionHandling = CLAY_TRANSITION_ALLOW_INTERACTIONS_WHILE_TRANSITIONING_POSITION;
        cfg.enter.setInitialState = ClayWidgets__FadeInInitialState;
        // TRIGGER (not SKIP) so a root-attached overlay fades in whenever it
        // appears, even on the very first frame its parent (the root) also
        // appears - otherwise a modal opened on frame 0 would pop in at full dim.
        cfg.enter.trigger = CLAY_TRANSITION_ENTER_TRIGGER_ON_FIRST_PARENT_FRAME;
    }
    return cfg;
}

// The resting "cleared" background for a surface that fades a hover/selected
// fill in and out. Returns the fill's own RGB with alpha 0 - NOT {0,0,0,0}.
// Because a color transition eases every channel, a {0,0,0,0} resting state
// would drag the RGB through black as the alpha ramps, so a fill appears to fade
// in from a dark flash instead of from its true hue. Keeping the RGB fixed and
// easing only the alpha fades cleanly from/to nothing. At rest (alpha 0) nothing
// is drawn, so this is visually identical to full transparency when idle.
static Clay_Color ClayWidgets__FadeToClear(Clay_Color fill) {
    fill.a = 0;
    return fill;
}

// Background-color transition used by interactive surfaces (buttons, list/table
// rows, tabs, cells) so hover / selected / pressed color changes fade in over a
// short ease instead of snapping. Clay retains the previous frame's rendered
// color per element id and eases toward the new target whenever it changes, so a
// widget only needs to drop this into its element's `.transition` field and keep
// setting `.backgroundColor` to the desired target as it already does.
//
// Returns a zeroed (handler == NULL) config when animations are disabled, which
// Clay treats as no transition at all - giving instant, deterministic colors for
// screenshots and honoring a reduce-motion preference.
static Clay_TransitionElementConfig ClayWidgets__ColorTransition(const ClayWidgets_Context *ctx) {
    Clay_TransitionElementConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (ctx && ctx->animationsEnabled) {
        cfg.handler = Clay_EaseOut;
        cfg.duration = CLAY_WIDGETS_ANIM_HOVER_DURATION;
        cfg.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
    }
    return cfg;
}

// Eases a per-id scalar toward `target` each frame and returns the current
// value - the Route B counterpart to Clay's declarative transitions, for effects
// that aren't a property of a single element (e.g. a toggle knob sliding across
// its track). `speed` is an exponential-smoothing rate per second, so the motion
// is frame-rate independent. On the first request for an id, or when its widget
// reappears after being absent, the value snaps to the target so widgets render
// settled instead of sweeping in from a stale value. Returns `target` unchanged
// when animations are disabled.
static float ClayWidgets__AnimTo(ClayWidgets_Context *ctx, uint32_t id, float target, float speed) {
    if (!ctx || !ctx->animationsEnabled || id == 0) {
        return target;
    }

    ClayWidgets_AnimSlot *slot = NULL;
    for (int32_t i = 0; i < CLAY_WIDGETS_MAX_ANIMS; ++i) {
        if (ctx->anims[i].id == id) {
            slot = &ctx->anims[i];
            break;
        }
    }

    // No slot yet: claim an empty or stale one, starting settled at the target.
    if (!slot) {
        for (int32_t i = 0; i < CLAY_WIDGETS_MAX_ANIMS; ++i) {
            if (ctx->anims[i].id == 0 || ctx->anims[i].frame + 1 < ctx->animFrame) {
                ctx->anims[i].id = id;
                ctx->anims[i].value = target;
                ctx->anims[i].frame = ctx->animFrame;
                return target;
            }
        }
        return target; // store full; skip animating rather than evict a live slot
    }

    // Present but not touched last frame: the widget reappeared, so restart
    // settled rather than sweeping from wherever it left off.
    if (slot->frame + 1 < ctx->animFrame) {
        slot->value = target;
        slot->frame = ctx->animFrame;
        return target;
    }

    // Already advanced this frame (a second call for the same id in one frame):
    // return the current value instead of easing again, which would apply dt
    // twice and make the motion frame-rate dependent.
    if (slot->frame == ctx->animFrame) {
        return slot->value;
    }

    // Skip easing on a non-positive or non-finite dt. `!(dt > 0.0f)` also rejects
    // NaN (any comparison with NaN is false); a bare `dt < 0.0f` would let a NaN
    // through and permanently poison slot->value.
    float dt = ctx->input.deltaTime;
    if (!(dt > 0.0f)) {
        dt = 0.0f;
    }
    float factor = 1.0f - expf(-dt * speed);
    slot->value += (target - slot->value) * factor;
    if (fabsf(target - slot->value) < 0.0015f) {
        slot->value = target;
    }
    slot->frame = ctx->animFrame;
    return slot->value;
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

    // Reserve a couple of pixels at the right so a caret sitting at the very end of
    // the text stays inside the clip instead of being scissored off its edge.
    const float caretPad = 2.0f;
    float totalWidth = ClayWidgets__MeasureWidth(ctx, text, length, fontId, fontSize, letterSpacing);
    if (totalWidth + caretPad <= availableWidth || availableWidth <= 0.0f) {
        ctx->textScrollX = 0.0f;
        return;
    }

    float maxScroll = totalWidth + caretPad - availableWidth;
    float caretX = ClayWidgets__MeasureWidth(ctx, text, ctx->textCursor, fontId, fontSize, letterSpacing);
    float scrollX = ClayWidgets__ClampF32(ctx->textScrollX, 0.0f, maxScroll);

    if (caretX < scrollX) {
        scrollX = caretX;
    } else if (caretX > scrollX + availableWidth - caretPad) {
        scrollX = caretX - availableWidth + caretPad;
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

// Hands a slice of a text-input buffer to the platform clipboard. Staged
// through clipboardScratch because the platform setter wants a NUL-terminated
// string and the selection is a slice of the caller's buffer.
static void ClayWidgets__CopyToClipboard(ClayWidgets_Context *ctx, const char *text, int32_t length) {
    if (!ctx || !ctx->setClipboardText || !text || length <= 0) {
        return;
    }
    int32_t cap = (int32_t)sizeof(ctx->clipboardScratch) - 1;
    if (length > cap) {
        length = cap;
    }
    memcpy(ctx->clipboardScratch, text, (size_t)length);
    ctx->clipboardScratch[length] = '\0';
    ctx->setClipboardText(ctx->clipboardScratch, ctx->clipboardUserData);
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

// Tab moves focus forward through last frame's registration order;
// Shift+Tab moves backward. Both wrap.
static void ClayWidgets__AdvanceFocus(ClayWidgets_Context *ctx) {
    if (!ctx || !ctx->input.keyTab || ctx->focusCount <= 0) {
        return;
    }

    int32_t currentIndex = ClayWidgets__FindFocusableIndex(ctx, ctx->focusedId);
    int32_t nextIndex;
    if (ctx->input.shiftDown) {
        nextIndex = (currentIndex > 0) ? currentIndex - 1 : ctx->focusCount - 1;
    } else {
        nextIndex = (currentIndex >= 0) ? (currentIndex + 1) % ctx->focusCount : 0;
    }
    ctx->focusedId = ctx->focusOrder[nextIndex];
}

static bool ClayWidgets__RegisterFocusable(ClayWidgets_Context *ctx, Clay_ElementId id, bool over) {
    if (!ctx) {
        return false;
    }

    // While a focus trap (modal) was active last frame, widgets outside it are
    // unreachable: they don't join the Tab order and give up keyboard focus,
    // so Enter can't activate a control behind the scrim.
    if (ctx->focusTrapPrevId != 0 && !ctx->insideFocusTrap) {
        if (ctx->focusedId == id.id) {
            ctx->focusedId = 0;
        }
        return false;
    }

    if (ctx->focusCount < CLAY_WIDGETS_MAX_FOCUSABLES) {
        ctx->focusOrder[ctx->focusCount++] = id.id;
    } else {
        ClayWidgets__ReportError(ctx, CLAY_WIDGETS__ERROR_FLAG_FOCUSABLES,
            "focus order full: more than CLAY_WIDGETS_MAX_FOCUSABLES interactive widgets in one "
            "frame; extra widgets are skipped by Tab. Define CLAY_WIDGETS_MAX_FOCUSABLES larger.");
    }

    if (ctx->input.pointerPressed && over) {
        ctx->focusedId = id.id;
    }

    return ctx->focusedId == id.id;
}

static bool ClayWidgets__ActivateFocused(ClayWidgets_Context *ctx, Clay_ElementId id) {
    return ctx && ctx->focusedId == id.id && ctx->input.keyEnter;
}

// Registers a hovered clip element whose Clay clip becomes a scroll container but
// can't consume a vertical wheel (a horizontally-clipped table, a single-line
// text field). Clay routes the wheel to the innermost clip under the pointer and
// drops it if that clip can't scroll in the wheel's direction, so without this
// the widget silently eats the scroll. Recorded here (during layout, using last
// frame's box) and acted on in the next BeginFrame, which forwards the wheel
// directly to the enclosing scroll panel. Call every frame the pointer is over
// the element.
static void ClayWidgets__RegisterWheelFallthrough(ClayWidgets_Context *ctx, Clay_ElementId clipId, bool pointerOver) {
    if (!ctx || !pointerOver) {
        return;
    }
    Clay_ElementData data = Clay_GetElementData(clipId);
    if (data.found) {
        ctx->wheelFallthroughId = clipId.id;
        ctx->wheelFallthroughBox = data.boundingBox;
        // The innermost scroll panel currently open around this clip is the one to
        // forward the wheel to. 0 if the clip isn't inside a scroll panel, or if
        // nesting ran past the stack (depth is counted unconditionally but only
        // stored up to the cap, so a depth beyond the cap has no recorded id -
        // guard the index to avoid reading past scrollPanelStack).
        ctx->wheelFallthroughPanelId =
            (ctx->scrollPanelDepth > 0 && ctx->scrollPanelDepth <= CLAY_WIDGETS_MAX_SCROLL_NESTING)
                ? ctx->scrollPanelStack[ctx->scrollPanelDepth - 1]
                : 0;
    }
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
    ctx->animationsEnabled = true;
}

void ClayWidgets_SetMeasureTextFunction(ClayWidgets_Context *ctx, ClayWidgets_MeasureTextFunction measureText, void *userData) {
    if (!ctx) {
        return;
    }
    ctx->measureText = measureText;
    ctx->measureTextUserData = userData;
}

void ClayWidgets_SetClipboardFunctions(
    ClayWidgets_Context *ctx,
    ClayWidgets_GetClipboardTextFunction getText,
    ClayWidgets_SetClipboardTextFunction setText,
    void *userData
) {
    if (!ctx) {
        return;
    }
    ctx->getClipboardText = getText;
    ctx->setClipboardText = setText;
    ctx->clipboardUserData = userData;
}

void ClayWidgets_SetErrorHandler(ClayWidgets_Context *ctx, ClayWidgets_ErrorHandlerFunction handler, void *userData) {
    if (!ctx) {
        return;
    }
    ctx->errorHandler = handler;
    ctx->errorHandlerUserData = userData;
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
    ctx->layoutDimensions = layoutSize;
    ctx->textScratchNext = 0;
    ctx->scrollPanelDepth = 0;
    ctx->frameErrorFlags = 0;
    // Roll the focus trap forward: registration during this frame's layout
    // checks last frame's trap (widgets before the modal in declaration order
    // have already registered by the time BeginModal runs).
    ctx->focusTrapPrevId = ctx->focusTrapId;
    ctx->focusTrapId = 0;
    ctx->insideFocusTrap = false;
    ctx->animFrame++;
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

    // While a widget has captured the pointer (a slider or scroll-bar thumb being
    // dragged, a held button), suppress pointer drag-scrolling so moving the mouse
    // adjusts that widget instead of also scrolling the panel underneath it. The
    // wheel (scrollDelta) still passes through. activeId persists across the drag,
    // so this holds for every frame after the initial press.
    bool dragScroll = enableDragScroll && ctx->activeId == 0;
    Clay_UpdateScrollContainers(dragScroll, scrollDelta, input.deltaTime);

    // A widget whose clip can't consume a vertical wheel (a horizontally-clipped
    // table, a single-line text field) registered itself last frame while
    // hovered. Clay just handed it the wheel and dropped it, so forward that
    // wheel straight to the scroll panel the widget lives in. Done directly
    // (rather than by moving the pointer) so it works even when the clip covers
    // the whole panel, leaving no bare panel pixel to reroute onto.
    if (scrollDelta.y != 0.0f && ctx->wheelFallthroughId != 0 && ctx->wheelFallthroughPanelId != 0
        && ClayWidgets__PointInBoundingBox(pointerPosition.x, pointerPosition.y, ctx->wheelFallthroughBox)) {
        Clay_ElementId panelId = CLAY__INIT(Clay_ElementId) CLAY__DEFAULT_STRUCT;
        panelId.id = ctx->wheelFallthroughPanelId;
        Clay_ScrollContainerData panelScroll = Clay_GetScrollContainerData(panelId);
        if (panelScroll.found && panelScroll.scrollPosition) {
            float maxScroll = panelScroll.contentDimensions.height - panelScroll.scrollContainerDimensions.height;
            if (maxScroll < 0.0f) {
                maxScroll = 0.0f;
            }
            float y = panelScroll.scrollPosition->y + scrollDelta.y * 10.0f; // match Clay's wheel step
            panelScroll.scrollPosition->y = ClayWidgets__Clamp(y, -maxScroll, 0.0f);
        }
    }
    // Consume the registration; hovered clip widgets re-register during this
    // frame's layout, so a stale entry can't keep scrolling after the pointer
    // leaves the widget.
    ctx->wheelFallthroughId = 0;

    Clay_BeginLayout();
}

Clay_RenderCommandArray ClayWidgets_EndFrame(ClayWidgets_Context *ctx) {
    // Forward the frame's own deltaTime so Clay transitions and the widget
    // animations (ClayWidgets__AnimTo) can never run on different clocks.
    return Clay_EndLayout(ctx ? ctx->input.deltaTime : 0.0f);
}

#endif

#endif