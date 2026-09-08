#ifndef CLAY_WIDGETS_TOOLTIP_H
#define CLAY_WIDGETS_TOOLTIP_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before tooltip.h"
#endif

#ifndef CLAY_WIDGETS_TOOLTIP_DELAY
#define CLAY_WIDGETS_TOOLTIP_DELAY 0.5f
#endif

// Attaches a hover tooltip to an already-declared element. Call it *after* the
// anchor element (identified by anchorId) has been closed, while its parent
// container is still open - the tooltip floats relative to the anchor by id, so
// it does not matter where in the sibling order it is emitted.
//
// The tooltip appears once the pointer has rested over the anchor for
// CLAY_WIDGETS_TOOLTIP_DELAY seconds and is drawn on top with pointer events
// passing through, so it never interferes with the control it describes.
//
//   ClayWidgets_Button(&ui, CLAY_ID("Save"), CLAY_STRING("Save"));
//   ClayWidgets_Tooltip(&ui, CLAY_ID("Save"), CLAY_STRING("Write changes to disk"));
void ClayWidgets_Tooltip(ClayWidgets_Context *ctx, Clay_ElementId anchorId, Clay_String text);
void ClayWidgets_TooltipEx(ClayWidgets_Context *ctx, Clay_ElementId anchorId, Clay_String text, float delaySeconds);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

// Per-anchor hover dwell, kept in the state pool only while the pointer is
// over the anchor. `lastFrame` detects a hover that lapsed: the pool slot may
// outlive the hover, so a stale stamp means the pointer left and came back
// and the timer must restart rather than resume.
typedef struct ClayWidgets__TooltipState {
    float dwell;
    uint32_t lastFrame;
} ClayWidgets__TooltipState;

void ClayWidgets_TooltipEx(ClayWidgets_Context *ctx, Clay_ElementId anchorId, Clay_String text, float delaySeconds) {
    if (!ctx || text.length <= 0 || !text.chars) {
        return;
    }

    if (!Clay_PointerOver(anchorId)) {
        return;
    }

    // Accumulate dwell time while hovered; sweeping across a row of controls
    // restarts each anchor's timer so tooltips don't flash. A NULL from an
    // exhausted pool means no timer, so the tooltip simply never appears.
    ClayWidgets__TooltipState *state =
        (ClayWidgets__TooltipState *)ClayWidgets_GetState(ctx, anchorId.id, (int32_t)sizeof(*state));
    if (!state) {
        return;
    }
    if (ctx->animFrame - state->lastFrame > 1) {
        state->dwell = 0.0f; // fresh slot, or the hover lapsed - restart
    } else {
        state->dwell += ctx->input.deltaTime;
    }
    state->lastFrame = ctx->animFrame;

    if (state->dwell < delaySeconds) {
        return;
    }

    Clay_ElementId tooltipId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTooltip"), anchorId.id);

    CLAY(tooltipId, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = {
                .left = ctx->theme.spacing.sm,
                .right = ctx->theme.spacing.sm,
                .top = ctx->theme.spacing.xs,
                .bottom = ctx->theme.spacing.xs,
            },
        },
        .backgroundColor = ctx->theme.surfaceColor,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
        .floating = {
            .offset = { .x = 0.0f, .y = 6.0f },
            .parentId = anchorId.id,
            .zIndex = ClayWidgets__OverlayZ(ctx, 200),
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
        },
        // A classic tooltip is a flat panel with a hard one-pixel outline, not a
        // beveled control - it isn't something you can press.
        .border = ClayWidgets__Border(ctx, ctx->theme.borderColor),
    }) {
        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeSmall,
            .wrapMode = CLAY_TEXT_WRAP_NONE,
        });
    }
}

void ClayWidgets_Tooltip(ClayWidgets_Context *ctx, Clay_ElementId anchorId, Clay_String text) {
    ClayWidgets_TooltipEx(ctx, anchorId, text, CLAY_WIDGETS_TOOLTIP_DELAY);
}

#endif

#endif
