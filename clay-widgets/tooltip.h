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

void ClayWidgets_TooltipEx(ClayWidgets_Context *ctx, Clay_ElementId anchorId, Clay_String text, float delaySeconds) {
    if (!ctx || text.length <= 0 || !text.chars) {
        return;
    }

    bool over = Clay_PointerOver(anchorId);

    // Track a single hovered anchor and accumulate dwell time. Moving to a new
    // anchor (or off all of them) restarts the timer so tooltips do not flash as
    // the pointer sweeps across a row of controls.
    if (over) {
        if (ctx->hoverTooltipId != anchorId.id) {
            ctx->hoverTooltipId = anchorId.id;
            ctx->hoverTooltipTime = 0.0f;
        } else {
            ctx->hoverTooltipTime += ctx->input.deltaTime;
        }
    } else if (ctx->hoverTooltipId == anchorId.id) {
        ctx->hoverTooltipId = 0;
        ctx->hoverTooltipTime = 0.0f;
    }

    bool show = over && ctx->hoverTooltipTime >= delaySeconds;
    if (!show) {
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
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
        .floating = {
            .offset = { .x = 0.0f, .y = 6.0f },
            .parentId = anchorId.id,
            .zIndex = 200,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
        },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
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
