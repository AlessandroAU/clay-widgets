#ifndef CLAY_WIDGETS_BUTTON_H
#define CLAY_WIDGETS_BUTTON_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before button.h"
#endif

typedef enum ClayWidgets_ButtonVariant {
    CLAY_WIDGETS_BUTTON_DEFAULT = 0,
    CLAY_WIDGETS_BUTTON_PRIMARY = 1,
    CLAY_WIDGETS_BUTTON_DANGER  = 2,
} ClayWidgets_ButtonVariant;
typedef struct ClayWidgets_ButtonOptions {
    ClayWidgets_ButtonVariant variant;
    bool disabled;
    // Optional sizing override. The zero value is CLAY_SIZING_FIT(0, 0) on
    // both axes, i.e. the default wrap-to-label behavior; set it for e.g. a
    // fixed square icon button or a full-width toolbar button.
    Clay_Sizing sizing;
} ClayWidgets_ButtonOptions;
bool ClayWidgets_ButtonEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, ClayWidgets_ButtonOptions options);

bool ClayWidgets_Button(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_ButtonEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, ClayWidgets_ButtonOptions options) {
    if (!ctx) {
        return false;
    }

    // Disabled: inert. No focus registration, no press/click tracking, always
    // returns false. Muted fill (surfaceAlt mixed halfway to surface), muted
    // text and a plain border, with no color transition.
    if (options.disabled) {
        Clay_Color color = ClayWidgets__MixColor(ctx->theme.surfaceAltColor, ctx->theme.surfaceColor, ctx->theme.disabledMix);

        CLAY(id, {
            .layout = {
                .sizing = options.sizing,
                .padding = CLAY_PADDING_ALL(ctx->theme.spacing.md),
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = color,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
            .border = {
                .color = ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            CLAY_TEXT(text, {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
                .textAlignment = CLAY_TEXT_ALIGN_CENTER,
            });
        }

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

    // Per-variant resting / hover / pressed fills and text color. DEFAULT keeps
    // the original theme-driven look; PRIMARY and DANGER carry their own palette.
    Clay_Color base;
    Clay_Color hover;
    Clay_Color pressed;
    Clay_Color textColor;
    switch (options.variant) {
        case CLAY_WIDGETS_BUTTON_PRIMARY:
            base = ctx->theme.accentColor;
            hover = ClayWidgets__MixColor(ctx->theme.accentColor, (Clay_Color){255, 255, 255, 255}, 0.12f);
            pressed = ClayWidgets__MixColor(ctx->theme.accentColor, (Clay_Color){0, 0, 0, 255}, 0.15f);
            textColor = ctx->theme.onAccentColor;
            break;
        case CLAY_WIDGETS_BUTTON_DANGER:
            // The theme's shared danger color (same as badges and toasts),
            // with hover/pressed derived the same way as PRIMARY.
            base = ctx->theme.dangerColor;
            hover = ClayWidgets__MixColor(ctx->theme.dangerColor, (Clay_Color){255, 255, 255, 255}, 0.12f);
            pressed = ClayWidgets__MixColor(ctx->theme.dangerColor, (Clay_Color){0, 0, 0, 255}, 0.15f);
            textColor = ctx->theme.onAccentColor;
            break;
        case CLAY_WIDGETS_BUTTON_DEFAULT:
        default:
            base = ctx->theme.surfaceAltColor;
            hover = ctx->theme.hoverColor;
            pressed = ctx->theme.pressedColor;
            textColor = ctx->theme.textColor;
            break;
    }

    Clay_Color color = base;
    if (active) {
        color = pressed;
    } else if (over) {
        color = hover;
    }

    CLAY(id, {
        .layout = {
            .sizing = options.sizing,
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.md),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = color,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
        .border = {
            .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
        .transition = ClayWidgets__ColorTransition(ctx),
    }) {
        CLAY_TEXT(text, {
            .textColor = textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .textAlignment = CLAY_TEXT_ALIGN_CENTER,
        });
    }

    return clicked;
}

bool ClayWidgets_Button(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text) {
    return ClayWidgets_ButtonEx(ctx, id, text, (ClayWidgets_ButtonOptions){CLAY_WIDGETS_BUTTON_DEFAULT, false});
}

#endif

#endif
