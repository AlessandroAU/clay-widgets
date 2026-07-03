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

#endif

#endif