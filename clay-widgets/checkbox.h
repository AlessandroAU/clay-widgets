#ifndef CLAY_WIDGETS_CHECKBOX_H
#define CLAY_WIDGETS_CHECKBOX_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before checkbox.h"
#endif

bool ClayWidgets_CheckboxEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value, bool disabled);

bool ClayWidgets_Checkbox(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_CheckboxEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value, bool disabled) {
    if (!ctx || !value) {
        return false;
    }

    // Disabled: inert. No focus registration, no click; always returns false.
    // The box keeps a plain border (never the focus ring), the check fill is a
    // muted accent, and the label text is muted.
    bool over = disabled ? false : Clay_PointerOver(id);
    bool focused = disabled ? false : ClayWidgets__RegisterFocusable(ctx, id, over);
    bool clicked = false;
    if (!disabled) {
        clicked = ClayWidgets__ConsumeClick(ctx, over);
        if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
            clicked = true;
        }
        if (clicked) {
            *value = !(*value);
        }
    }

    Clay_Color checkFill = disabled
        ? ClayWidgets__MixColor(ctx->theme.accentColor, ctx->theme.surfaceColor, 0.4f)
        : ctx->theme.accentColor;
    Clay_Color labelColor = disabled ? ctx->theme.textMutedColor : ctx->theme.textColor;

    // When checked, the whole box becomes the accent fill — a single rounded rect —
    // rather than nesting a smaller filled rect inside the box. A nested fill can't
    // share the box's rounded corners exactly (Clay's border consumes no layout
    // space, so padding alone never lines the inner rect's corners up with the outer
    // curve), and the blue bleeds past the corners. One rect has no such seam. The
    // border folds into the fill so the box reads as a solid chip, except when it
    // should show the focus ring.
    Clay_Color boxBg = *value ? checkFill : ctx->theme.surfaceAltColor;
    Clay_Color boxBorder = (!disabled && (focused || over))
        ? ctx->theme.focusRingColor
        : (*value ? checkFill : ctx->theme.borderColor);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_FIXED(20),
                    .height = CLAY_SIZING_FIXED(20),
                },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = boxBg,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
            .border = {
                .color = boxBorder,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            if (*value) {
                CLAY_TEXT(CLAY_STRING("X"), {
                    .textColor = (Clay_Color){240, 248, 255, 255},
                    .fontId = ctx->theme.fontBody,
                    .fontSize = 14,
                });
            }
        }

        CLAY_TEXT(text, {
            .textColor = labelColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
        });
    }

    return clicked;
}

bool ClayWidgets_Checkbox(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value) {
    return ClayWidgets_CheckboxEx(ctx, id, text, value, false);
}

#endif

#endif
