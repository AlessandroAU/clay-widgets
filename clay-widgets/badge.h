#ifndef CLAY_WIDGETS_BADGE_H
#define CLAY_WIDGETS_BADGE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before badge.h"
#endif

// A badge / chip / tag: a small, rounded, filled pill of text used for statuses
// and counts. Display-only (no interaction). ClayWidgets_Badge picks colors
// from a variant; ClayWidgets_BadgeColor takes explicit background/text colors.
// The semantic variants draw from the theme's status palette
// (successColor/warningColor/dangerColor), shared with toasts and danger
// buttons so a status reads as the same color everywhere.
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

// Maps a semantic variant to its theme fill. Shared by badges and toasts so
// the two always agree on what "danger" looks like.
static Clay_Color ClayWidgets__SemanticColor(const ClayWidgets_Context *ctx, ClayWidgets_BadgeVariant variant) {
    switch (variant) {
        case CLAY_WIDGETS_BADGE_SUCCESS: return ctx->theme.successColor;
        case CLAY_WIDGETS_BADGE_WARNING: return ctx->theme.warningColor;
        case CLAY_WIDGETS_BADGE_DANGER:  return ctx->theme.dangerColor;
        case CLAY_WIDGETS_BADGE_NEUTRAL: return ctx->theme.borderColor;
        case CLAY_WIDGETS_BADGE_ACCENT:
        default:                         return ctx->theme.accentColor;
    }
}

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
        // Large radius clamps to a full pill in the renderer; a square-cornered
        // theme gets a square chip instead.
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm > 0 ? 999.0f : 0.0f),
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
    Clay_Color background;
    Clay_Color textColor;
    switch (variant) {
        case CLAY_WIDGETS_BADGE_ACCENT:
            background = ctx->theme.accentColor;
            textColor = ctx->theme.surfaceColor;
            break;
        case CLAY_WIDGETS_BADGE_NEUTRAL:
            background = ctx->theme.hoverColor;
            textColor = ctx->theme.textMutedColor;
            break;
        default:
            background = ClayWidgets__SemanticColor(ctx, variant);
            textColor = ctx->theme.onAccentColor;
            break;
    }
    ClayWidgets_BadgeColor(ctx, text, background, textColor);
}

#endif

#endif
