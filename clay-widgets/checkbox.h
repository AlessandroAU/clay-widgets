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
    disabled = disabled || ctx->disabledDepth > 0;


    // Disabled: inert. No focus registration, no click; always returns false.
    // The box keeps a plain border (never the focus ring), the check fill is a
    // muted accent, and the label text is muted.
    bool over = disabled ? false : Clay_PointerOver(id);
    if (over) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
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
        ? ClayWidgets__MixColor(ctx->theme.accentColor, ctx->theme.surfaceColor, ctx->theme.disabledMix)
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
    Clay_Color markColor = ctx->theme.onAccentColor;

    // A classic check box is a sunken white well with a mark in it, never a
    // filled chip: the 3D edge is what says "checkable", so the well stays
    // paper white and the tick is drawn in the text color.
    Clay_ElementId boxId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsCheckboxBox"), 0);
    if (ClayWidgets__IsBeveled(ctx)) {
        boxBg = disabled
            ? ClayWidgets__MixColor(ctx->theme.fieldColor, ctx->theme.surfaceColor, ctx->theme.disabledMix)
            : ctx->theme.fieldColor;
        markColor = labelColor;
        ClayWidgets_SetEdge(ctx, boxId, CLAY_WIDGETS_EDGE_SUNKEN);
        if (focused) {
            ClayWidgets__FocusRect(ctx, boxId);
        }
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY(boxId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_FIXED(20),
                    .height = CLAY_SIZING_FIXED(20),
                },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = boxBg,
            .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
            .border = ClayWidgets__Border(ctx, boxBorder),
        }) {
            if (*value) {
                CLAY_TEXT(CLAY_STRING("X"), {
                    .textColor = markColor,
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

    ClayWidgets__Describe(ctx,id,CLAY_WIDGETS_ROLE_CHECKBOX,text,*value,disabled);
    return clicked;
}

bool ClayWidgets_Checkbox(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value) {
    return ClayWidgets_CheckboxEx(ctx, id, text, value, false);
}

#endif

#endif
