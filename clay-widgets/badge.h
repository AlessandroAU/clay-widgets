#ifndef CLAY_WIDGETS_BADGE_H
#define CLAY_WIDGETS_BADGE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before badge.h"
#endif

// A badge / chip / tag: a small, rounded, filled pill of text used for statuses
// and counts. Display-only (no interaction). ClayWidgets_Badge picks colors
// from a variant; ClayWidgets_BadgeColor takes explicit background/text colors.
// The semantic variants use fixed status colors because the theme has no
// success/warning/danger palette of its own.
typedef enum ClayWidgets_BadgeVariant {
    CLAY_WIDGETS_BADGE_NEUTRAL = 0,
    CLAY_WIDGETS_BADGE_ACCENT = 1,
    CLAY_WIDGETS_BADGE_SUCCESS = 2,
    CLAY_WIDGETS_BADGE_WARNING = 3,
    CLAY_WIDGETS_BADGE_DANGER = 4,
} ClayWidgets_BadgeVariant;

void ClayWidgets_BadgeColor(ClayWidgets_Context *ctx, Clay_String text, Clay_Color background, Clay_Color textColor);
void ClayWidgets_Badge(ClayWidgets_Context *ctx, Clay_String text, ClayWidgets_BadgeVariant variant);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

void ClayWidgets_BadgeColor(ClayWidgets_Context *ctx, Clay_String text, Clay_Color background, Clay_Color textColor) {
    if (!ctx || text.length <= 0 || !text.chars) {
        return;
    }
    CLAY_AUTO_ID({
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = {
                .left = ctx->theme.spacing.sm,
                .right = ctx->theme.spacing.sm,
                .top = ctx->theme.spacing.xs,
                .bottom = ctx->theme.spacing.xs,
            },
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = background,
        // Large radius clamps to a full pill in the renderer.
        .cornerRadius = CLAY_CORNER_RADIUS(999),
    }) {
        CLAY_TEXT(text, {
            .textColor = textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeSmall,
            .wrapMode = CLAY_TEXT_WRAP_NONE,
        });
    }
}

void ClayWidgets_Badge(ClayWidgets_Context *ctx, Clay_String text, ClayWidgets_BadgeVariant variant) {
    if (!ctx) {
        return;
    }
    Clay_Color light = { 245, 248, 252, 255 };
    Clay_Color background;
    Clay_Color textColor;
    switch (variant) {
        case CLAY_WIDGETS_BADGE_ACCENT:
            background = ctx->theme.accentColor;
            textColor = ctx->theme.surfaceColor;
            break;
        case CLAY_WIDGETS_BADGE_SUCCESS:
            background = (Clay_Color){ 46, 160, 67, 255 };
            textColor = light;
            break;
        case CLAY_WIDGETS_BADGE_WARNING:
            background = (Clay_Color){ 191, 135, 0, 255 };
            textColor = light;
            break;
        case CLAY_WIDGETS_BADGE_DANGER:
            background = (Clay_Color){ 207, 54, 54, 255 };
            textColor = light;
            break;
        case CLAY_WIDGETS_BADGE_NEUTRAL:
        default:
            background = ctx->theme.hoverColor;
            textColor = ctx->theme.textMutedColor;
            break;
    }
    ClayWidgets_BadgeColor(ctx, text, background, textColor);
}

#endif

#endif
