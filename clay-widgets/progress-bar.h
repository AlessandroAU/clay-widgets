#ifndef CLAY_WIDGETS_PROGRESS_BAR_H
#define CLAY_WIDGETS_PROGRESS_BAR_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before progress-bar.h"
#endif

void ClayWidgets_ProgressBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    float progress01,
    Clay_String label
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

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

    // The trough is a well the fill sits in. Its padding has to clear the 3D
    // edge, or a full bar would paint over the inner band.
    Clay_ElementId troughId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsProgressTrough"), 0);
    uint16_t troughInset = ClayWidgets__IsBeveled(ctx) ? 2 : 1;
    ClayWidgets_SetEdge(ctx, troughId, CLAY_WIDGETS_EDGE_SUNKEN);

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

        CLAY(troughId, {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(ClayWidgets__IsBeveled(ctx) ? 14.0f : 12.0f) },
                .padding = CLAY_PADDING_ALL(troughInset),
            },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
            .border = ClayWidgets__Border(ctx, ctx->theme.borderColor),
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = CLAY_SIZING_PERCENT(t), .height = CLAY_SIZING_GROW(0) },
                },
                .backgroundColor = ctx->theme.accentColor,
                .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm > 0 ? (float)(ctx->theme.radiusSm - 1) : 0.0f),
            }) {}
        }
    }
}

#endif

#endif