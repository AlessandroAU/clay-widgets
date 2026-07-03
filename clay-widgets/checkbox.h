#ifndef CLAY_WIDGETS_CHECKBOX_H
#define CLAY_WIDGETS_CHECKBOX_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before checkbox.h"
#endif

bool ClayWidgets_Checkbox(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_Checkbox(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value) {
    if (!ctx || !value) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);
    if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
        clicked = true;
    }
    if (clicked) {
        *value = !(*value);
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
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_FIXED(20),
                    .height = CLAY_SIZING_FIXED(20),
                },
                .padding = CLAY_PADDING_ALL(1),
            },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
            .clip = { .horizontal = true, .vertical = true },
            .border = {
                .color = (focused || over) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            if (*value) {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_GROW(0),
                        },
                        .padding = CLAY_PADDING_ALL(3),
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = ctx->theme.accentColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm > 0 ? ctx->theme.radiusSm - 1 : 0),
                    .clip = { .horizontal = true, .vertical = true },
                }) {
                    CLAY_TEXT(CLAY_STRING("X"), {
                        .textColor = (Clay_Color){240, 248, 255, 255},
                        .fontId = ctx->theme.fontBody,
                        .fontSize = 14,
                    });
                }
            }
        }

        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
        });
    }

    return clicked;
}

#endif

#endif