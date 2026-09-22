#ifndef CLAY_WIDGETS_NUMBER_INPUT_H
#define CLAY_WIDGETS_NUMBER_INPUT_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before number-input.h"
#endif

typedef struct ClayWidgets_NumberInputOptions {
    int32_t minValue, maxValue, step;
    bool disabled;
} ClayWidgets_NumberInputOptions;

// Editable integer. Valid in-range edits apply immediately; Enter or blur clamps
// out-of-range drafts and restores empty drafts. Up/Down step; Escape resets the
// displayed draft to the current value. Disabled inputs never modify the value.
bool ClayWidgets_NumberInput(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_String label, int32_t *value, ClayWidgets_NumberInputOptions options);

#ifdef CLAY_WIDGETS_IMPLEMENTATION
#include <stdio.h>
typedef struct ClayWidgets__NumberState {
    char text[32];
    int32_t last;
    uint32_t frame;
    bool initialized, focused, refresh;
} ClayWidgets__NumberState;

static bool ClayWidgets__NumberText(const char *text, int32_t length, void *data) {
    (void)data;
    int32_t i = length && (text[0] == '-' || text[0] == '+') ? 1 : 0;
    for (; i < length; ++i) if (text[i] < '0' || text[i] > '9') return false;
    return true;
}

// Saturate during parsing, including drafts longer than a machine integer.
static bool ClayWidgets__ParseNumber(const char *text, int32_t lo, int32_t hi,
    int32_t *value, bool *inRange) {
    bool negative = *text == '-';
    if (*text == '-' || *text == '+') ++text;
    if (!*text) return false;
    int64_t magnitude = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        if (magnitude < 2147483648LL) magnitude = magnitude * 10 + (*text - '0');
    }
    int64_t parsed = negative ? -magnitude : magnitude;
    *inRange = parsed >= lo && parsed <= hi;
    *value = (int32_t)(parsed < lo ? lo : parsed > hi ? hi : parsed);
    return true;
}

bool ClayWidgets_NumberInput(ClayWidgets_Context *ctx, Clay_ElementId id,
    Clay_String label, int32_t *value, ClayWidgets_NumberInputOptions options) {
    if (!ctx || !value) return false;
    ClayWidgets__NumberState *state = (ClayWidgets__NumberState *)ClayWidgets_GetState(ctx, id.id, sizeof(ClayWidgets__NumberState));
    if (!state) return false;
    if (state->initialized && ctx->animFrame - state->frame > 1) {
        state->initialized = false;
        state->focused = false;
    }
    state->frame = ctx->animFrame;
    bool disabled = options.disabled || ctx->disabledDepth > 0;
    int32_t lo = options.minValue, hi = options.maxValue;
    if (hi < lo) { int32_t tmp = lo; lo = hi; hi = tmp; }
    int32_t before = *value;
    if (!disabled) *value = *value < lo ? lo : *value > hi ? hi : *value;
    if (!state->initialized || state->last != *value || disabled || state->refresh) {
        snprintf(state->text, sizeof(state->text), "%d", (int)*value);
        state->initialized = true;
        state->refresh = false;
    }
    ClayWidgets_EditResult result = {0};
    ClayWidgets_TextInputOptions textOptions = {0};
    textOptions.disabled = disabled;
    textOptions.validate = ClayWidgets__NumberText;
    textOptions.result = &result;
    ClayWidgets_TextInput(ctx, id, label, state->text, sizeof(state->text), textOptions);
    bool focused = !disabled && ctx->focusedId == id.id;
    if (!disabled) {
        bool commit = result.submitted || (state->focused && !focused);
        int32_t parsed = *value;
        bool inRange = false;
        if (!result.cancelled && ClayWidgets__ParseNumber(state->text, lo, hi, &parsed, &inRange)
            && (commit || (result.changed && inRange))) *value = parsed;
        bool stepping = focused && (ctx->input.keyUp || ctx->input.keyDown);
        if (stepping) {
            int64_t step = options.step > 0 ? options.step : 1;
            int64_t next = (int64_t)*value + (ctx->input.keyUp ? step : -step);
            *value = (int32_t)(next < lo ? lo : next > hi ? hi : next);
        }
        if (commit || result.cancelled || stepping) {
            // Clay holds this frame's text pointer and length through rendering.
            state->refresh = true;
        }
    }
    state->focused = focused;
    state->last = *value;
    return !disabled && before != *value;
}
#endif
#endif
