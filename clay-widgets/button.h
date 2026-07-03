#ifndef CLAY_WIDGETS_BUTTON_H
#define CLAY_WIDGETS_BUTTON_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before button.h"
#endif

bool ClayWidgets_Button(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_Button(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text) {
    if (!ctx) {
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

    Clay_Color color = ctx->theme.surfaceAltColor;
    if (active) {
        color = ctx->theme.pressedColor;
    } else if (over) {
        color = ctx->theme.hoverColor;
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.md),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = color,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
        .border = {
            .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .textAlignment = CLAY_TEXT_ALIGN_CENTER,
        });
    }

    return clicked;
}

#endif

#endif