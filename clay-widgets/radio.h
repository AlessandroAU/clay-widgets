#ifndef CLAY_WIDGETS_RADIO_H
#define CLAY_WIDGETS_RADIO_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before radio.h"
#endif

bool ClayWidgets_Radio(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_Radio(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue
) {
    if (!ctx || !selectedValue) {
        return false;
    }

    bool selected = (*selectedValue == optionValue);
    bool over = Clay_PointerOver(id);
    if (over) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);
    if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
        clicked = true;
    }

    bool changed = clicked && !selected;
    if (clicked) {
        *selectedValue = optionValue;
        selected = true;
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(20), .height = CLAY_SIZING_FIXED(20) } },
            // Round, so it keeps its flat outline under every theme - the edge
            // painter draws rectangles and would square off the circle.
            .backgroundColor = ctx->theme.fieldColor,
            .cornerRadius = CLAY_CORNER_RADIUS(10),
            .border = {
                .color = (focused || over) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            if (selected) {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_FIXED(10), .height = CLAY_SIZING_FIXED(10) },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = ctx->theme.accentColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                    .floating = {
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER,
                        },
                        .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                        .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                    },
                }) {}
            }
        }

        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
        });
    }

    ClayWidgets__Describe(ctx,id,CLAY_WIDGETS_ROLE_CHECKBOX,text,selected,false);
    return changed;
}

#endif

#endif
