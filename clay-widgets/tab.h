#ifndef CLAY_WIDGETS_TAB_H
#define CLAY_WIDGETS_TAB_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before tab.h"
#endif

typedef enum ClayWidgets_TabStyle {
    // Standalone segmented / navigation tab: a filled, fully-rounded pill.
    CLAY_WIDGETS_TAB_STYLE_PILL = 0,
    // Tab that lives inside a tab-plane header: no fill, top-rounded, with an
    // accent underline on the active tab so it reads as part of the framed
    // surface below rather than a free-floating button.
    CLAY_WIDGETS_TAB_STYLE_ATTACHED = 1,
} ClayWidgets_TabStyle;

// A single tab in a tab strip. Behaves like a radio: pass a distinct
// optionValue per tab and a shared selectedValue pointer. Returns true on the
// frame the tab becomes selected.
//
// ClayWidgets_Tab draws a standalone pill (good for a top navigation bar).
// ClayWidgets_TabEx lets you pick CLAY_WIDGETS_TAB_STYLE_ATTACHED, which is
// meant to sit in the header row of a framed "tab plane" (see the demo): lay
// several out in a horizontal strip directly above a bordered content body and
// switch the body on *selectedValue.
bool ClayWidgets_Tab(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue
);

bool ClayWidgets_TabEx(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue,
    ClayWidgets_TabStyle style
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_TabEx(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue,
    ClayWidgets_TabStyle style
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

    Clay_Color transparent = { 0, 0, 0, 0 };

    // Classic tabs are raised plates whether or not they're the current one -
    // the selected tab reads as selected because it carries the page's own
    // surface color, not because of an underline.
    bool beveled = ClayWidgets__IsBeveled(ctx);
    ClayWidgets_SetEdge(ctx, id, CLAY_WIDGETS_EDGE_RAISED);
    if (focused) {
        ClayWidgets__FocusRect(ctx, id);
    }

    if (style == CLAY_WIDGETS_TAB_STYLE_ATTACHED) {
        Clay_Color background = beveled ? ctx->theme.surfaceAltColor : ClayWidgets__FadeToClear(ctx->theme.hoverColor);
        if (selected) {
            background = ctx->theme.surfaceColor;
        } else if (over) {
            background = ctx->theme.hoverColor;
        }

        Clay_Color underlineColor = transparent;
        uint16_t underlineWidth = 0;
        if (selected) {
            underlineColor = ctx->theme.accentColor;
            underlineWidth = 2;
        } else if (focused) {
            underlineColor = ctx->theme.focusRingColor;
            underlineWidth = 2;
        }

        Clay_Color labelColor = ctx->theme.textMutedColor;
        if (selected) {
            labelColor = beveled ? ctx->theme.textColor : ctx->theme.accentColor;
        } else if (over) {
            labelColor = ctx->theme.textColor;
        }

        CLAY(id, {
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
                .padding = { .left = ctx->theme.spacing.lg, .right = ctx->theme.spacing.lg, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = background,
            .cornerRadius = { .topLeft = (float)ctx->theme.radiusSm, .topRight = (float)ctx->theme.radiusSm, .bottomLeft = 0, .bottomRight = 0 },
            .border = ClayWidgets__EdgeBorder(ctx, underlineColor, CLAY__INIT(Clay_BorderWidth){ 0, 0, 0, underlineWidth, 0 }),
            .transition = ClayWidgets__ColorTransition(ctx),
        }) {
            CLAY_TEXT(text, {
                .textColor = labelColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
                .textAlignment = CLAY_TEXT_ALIGN_CENTER,
            });
        }

        return changed;
    }

    Clay_Color background = ctx->theme.surfaceAltColor;
    if (selected) {
        background = beveled ? ctx->theme.pressedColor : ctx->theme.accentColor;
    } else if (over) {
        background = ctx->theme.hoverColor;
    }
    if (beveled && selected) {
        ClayWidgets_SetEdge(ctx, id, CLAY_WIDGETS_EDGE_SUNKEN);
    }

    Clay_Color borderColor = ctx->theme.borderColor;
    if (selected) {
        borderColor = ctx->theme.accentColor;
    } else if (focused) {
        borderColor = ctx->theme.focusRingColor;
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = { .left = ctx->theme.spacing.lg, .right = ctx->theme.spacing.lg, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = background,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusMd),
        .border = ClayWidgets__Border(ctx, borderColor),
        .transition = ClayWidgets__ColorTransition(ctx),
    }) {
        CLAY_TEXT(text, {
            .textColor = (selected && !beveled) ? ctx->theme.surfaceColor : ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .textAlignment = CLAY_TEXT_ALIGN_CENTER,
        });
    }

    return changed;
}

bool ClayWidgets_Tab(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String text,
    int32_t optionValue,
    int32_t *selectedValue
) {
    return ClayWidgets_TabEx(ctx, id, text, optionValue, selectedValue, CLAY_WIDGETS_TAB_STYLE_PILL);
}

#endif

#endif
